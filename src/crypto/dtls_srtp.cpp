/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <array>
#include <bctoolbox/defs.h>
#include <mutex>
#include <queue>

#include "mediastreamer2/dtls_srtp.h"
#include "mediastreamer2/mediastream.h"

#ifdef _WIN32
#include <malloc.h>
#endif

#include "bctoolbox/crypto.h"

namespace {
/** A class to manage all crypto contexts needed by Dtls-Srtp */
class DtlsCrypto {
public:
	bctbx_x509_certificate_t *crt;
	bctbx_ssl_config_t *ssl_config;
	bctbx_ssl_context_t *ssl;
	bctbx_rng_context_t *rng;
	bctbx_signing_key_t *pkey;

	DtlsCrypto()
	    : crt(bctbx_x509_certificate_new()), ssl_config(bctbx_ssl_config_new()), ssl(nullptr),
	      rng(bctbx_rng_context_new()), pkey(bctbx_signing_key_new()) {
	}
	~DtlsCrypto() {
		bctbx_rng_context_free(rng);
		bctbx_signing_key_free(pkey);
		bctbx_x509_certificate_free(crt);
		bctbx_ssl_context_free(ssl);
		bctbx_ssl_config_free(ssl_config);
	}
};

/* DTLS only allow use of AES128 so we have 16 bytes key and 14 byte salt in any case */
constexpr size_t DtlsSrtpKeyLen = 16;
constexpr size_t DtlsSrtpSaltLen = 14;
// key material generated during the DTLS handshake: a pair of key+salt : one for reception, one for sending
constexpr size_t DtlsSrtpKeyMaterial = 2 * (DtlsSrtpKeyLen + DtlsSrtpSaltLen);
// period in ms to trigger the repetition timer
constexpr uint64_t DtlsRepetitionTimerPoll = 100;

/* Do not modify this values: fingerprint_verified MUST be > handshake_over*/
enum class DtlsStatus : uint8_t {
	ContextNotReady = 0,
	ContextReady = 1,
	HandshakeOngoing = 2,
	HandshakeOver = 3,
	FingerprintVerified = 4,
};
} // anonymous namespace

struct _MSDtlsSrtpContext {
	MSMediaStreamSessions *mStreamSessions;
	MSDtlsSrtpRole mRole;         /**< can be unset(at init on caller side), client or server */
	std::string mPeerFingerprint; /**< used to store peer fingerprint passed through SDP */
	int mtu;
	RtpTransportModifier *rtp_modifier;
	DtlsCrypto mDtlsCryptoContext; /**< a structure containing all contexts needed by DTLS handshake for RTP channel */
	DtlsStatus mChannelStatus;     /**< channel status :not ready, ready, hanshake on going, handshake over, fingerprint
	                                  verified */
	std::array<uint8_t, DtlsSrtpKeyMaterial> mSrtpKeyMaterial; /**< Store the key material generated by the handshake
	                                                                 on rtp channel */
	MSCryptoSuite mSrtpProtectionProfile;                      /**< agreed protection profile on rtp channel */
	std::queue<std::vector<uint8_t>>
	    mRtpIncomingBuffer;      /**< buffer of incoming DTLS packet to be read by mbedtls callback */
	uint64_t rtp_time_reference; /**< an epoch in ms, used to manage retransmission when we are client */
	bool retry_sending;          /**< a flag to set a retry after failed packet sending */
	std::mutex mtx;              /**< lock any operation on this context */

	_MSDtlsSrtpContext() = delete;
	_MSDtlsSrtpContext(MSMediaStreamSessions *sessions, MSDtlsSrtpParams *params) {
		mRole = params->role;
		mtu = params->mtu;
		rtp_time_reference = 0;
		retry_sending = false;

		mStreamSessions = sessions;
		mChannelStatus = DtlsStatus::ContextNotReady;
		mSrtpProtectionProfile = MS_CRYPTO_SUITE_INVALID;
	};
	~_MSDtlsSrtpContext() = default;

	int initialiseDtlsCryptoContext(MSDtlsSrtpParams *params);
	void start();
	void createSslContext();
	void setRole(MSDtlsSrtpRole role);
	void checkChannelStatus();
	void setKeyMaterial();
	int processDtlsPacket(mblk_t *msg);
};

namespace {
/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/
// Fingerprint size should never be more than 255 (actually less than that with supported types:
// SHA-512 is 64 bytes long -> 3*64 for its hexa and : representation +8 for the prefix = 200
constexpr size_t maxFingerPrintSize = 255;

/**************************/
/**** Helper functions ****/
// case-insensitive prefix comparison of fingerprints
bool startsWithCaseInsensitive(const std::string &fingerprint, const char *prefix, size_t prefixSize) {
	if (fingerprint.size() < prefixSize) {
		return false;
	}
	for (size_t i = 0; i < prefixSize; ++i) {
		if (std::tolower(static_cast<unsigned char>(fingerprint[i])) != prefix[i]) {
			return false;
		}
	}
	return true;
}
// case-insensitive compare for whole fingerprints
bool caseInsensitiveCompare(const std::string &fingerprint, const char *cStr) {
	size_t cStrLen = strlen(cStr);
	if (fingerprint.size() != cStrLen) {
		return false;
	}
	for (size_t i = 0; i < cStrLen; ++i) {
		if (std::tolower(static_cast<unsigned char>(fingerprint[i])) !=
		    std::tolower(static_cast<unsigned char>(cStr[i]))) {
			return false;
		}
	}
	return true;
}
/**
 * @Brief Compute the certificate fingerprint(hash of DER formated certificate)
 * hash function to use shall be the same used by certificate signature(this is a way to ensure that the hash function
 * is available at both ends as they already agreed on certificate) However, peer may provide a fingerprint generated
 * with another hash function(indicated at the fingerprint header). In case certificate and fingerprint hash function
 * differs, issue a warning and use the fingerprint one
 *
 * @param[in]	certificate		Certificate we shall compute the fingerprint
 * @param[in]	peer_fingerprint	Fingerprint received from peer, check its header to get the hash function used to
 * generate it
 *
 * @return 0 if the fingerprint doesn't match, 1 is they do.
 */
uint8_t ms_dtls_srtp_check_certificate_fingerprint(const bctbx_x509_certificate_t *certificate,
                                                   const std::string &peer_fingerprint) {
	char fingerprint[256]; /* maximum length of the fingerprint for sha-512: 8+3*64+1 so we're good with 256 bytes
	                          buffer */
	bctbx_md_type_t hash_function = BCTBX_MD_UNDEFINED;
	bctbx_md_type_t certificate_signature_hash_function = BCTBX_MD_UNDEFINED;
	int32_t ret = 0;

	/* get Hash algorithm used from peer fingerprint */
	if (startsWithCaseInsensitive(peer_fingerprint, "sha-1 ", 6)) {
		hash_function = BCTBX_MD_SHA1;
	} else if (startsWithCaseInsensitive(peer_fingerprint, "sha-224 ", 8)) {
		hash_function = BCTBX_MD_SHA224;
	} else if (startsWithCaseInsensitive(peer_fingerprint, "sha-256 ", 8)) {
		hash_function = BCTBX_MD_SHA256;
	} else if (startsWithCaseInsensitive(peer_fingerprint, "sha-384 ", 8)) {
		hash_function = BCTBX_MD_SHA384;
	} else if (startsWithCaseInsensitive(peer_fingerprint, "sha-512 ", 8)) {
		hash_function = BCTBX_MD_SHA512;
	} else { /* we have an unknown hash function: return null */
		ms_error("DTLS-SRTP received invalid peer fingerprint %s, hash function unknown", peer_fingerprint.c_str());
		return 0;
	}

	/* retrieve the one used for the certificate signature */
	bctbx_x509_certificate_get_signature_hash_function(certificate, &certificate_signature_hash_function);

	/* check that hash function used match the one used for certificate signature */
	if (hash_function != certificate_signature_hash_function) {
		ms_warning("DTLS-SRTP peer fingerprint generated using a different hash function that the one used for "
		           "certificate signature, peer is nasty but lucky we have the hash function required anyway");
	}

	/* compute the fingerprint using the requested hash function */
	ret = bctbx_x509_certificate_get_fingerprint(certificate, fingerprint, 255, hash_function);
	if (ret <= 0) {
		ms_error(
		    "DTLS Handshake successful but unable to compute peer certificate fingerprint : bctoolbox returns [-0x%x]",
		    -ret);
	}

	/* compare fingerprints */
	if (caseInsensitiveCompare(peer_fingerprint, (char *)fingerprint)) {
		return 1;
	} else {
		ms_error("DTLS Handshake successful but fingerprints differ received : %s computed %s",
		         peer_fingerprint.c_str(), fingerprint);
		return 0;
	}
}

/**
 * Convert a bctoolbox defined value for SRTP protection profile to the mediastreamer enumeration of SRTP protection
 * profile
 * @param[in]	dtls_srtp_protection_profile	A DTLS-SRTP protection profile defined by bctoolbox
 * @return the matching profile defined in mediatream.h
 */
MSCryptoSuite
ms_dtls_srtp_bctbx_protection_profile_to_ms_crypto_suite(bctbx_dtls_srtp_profile_t dtls_srtp_protection_profile) {
	switch (dtls_srtp_protection_profile) {
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_80:
			return MS_AES_128_SHA1_80;
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_32:
			return MS_AES_128_SHA1_32;
		case BCTBX_SRTP_NULL_HMAC_SHA1_80:
			return MS_AES_128_SHA1_80_NO_CIPHER;
		case BCTBX_SRTP_NULL_HMAC_SHA1_32: /* this profile is defined in DTLS-SRTP rfc but not implemented by libsrtp */
			return MS_CRYPTO_SUITE_INVALID;
		default:
			return MS_CRYPTO_SUITE_INVALID;
	}
}

void schedule_rtp(struct _RtpTransportModifier *t) {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;
	/* the retry sending flag is raised when a sending failed */
	if (ctx->retry_sending) {
		std::lock_guard<std::mutex> lock(ctx->mtx);
		ctx->retry_sending = false;
		bctbx_ssl_handshake(ctx->mDtlsCryptoContext.ssl);
		return;
	}
	/* the retransmission timer increasing value is managed by the crypto lib
	 * just poke it each 100ms */
	if (ctx->rtp_time_reference > 0) { /* only when retransmission timer is armed */
		auto current_time = bctbx_get_cur_time_ms();
		if (current_time - ctx->rtp_time_reference > DtlsRepetitionTimerPoll) {
			std::lock_guard<std::mutex> lock(ctx->mtx);
			if (ctx->rtp_time_reference >
			    0) { /* recheck the timer is still armed once we're into the guarded section */
				bctbx_ssl_handshake(ctx->mDtlsCryptoContext.ssl);
				ctx->rtp_time_reference = bctbx_get_cur_time_ms();
			}
		}
	}
}

/********************************************/
/**** bctoolbox DTLS packet I/O functions ****/

/**
 * Send a DTLS packet via RTP.
 *
 * DTLS calls this method to send a DTLS packet via the RTP session.
 *
 * @param ctx
 *    Pointer to the MSDtlsSrtpContext structure.
 * @param data
 *    Points to DTLS message to send.
 * @param length
 *    The length in bytes of the data
 * @return
 *    length of data sent
 */
int ms_dtls_srtp_rtp_sendData(void *ctx, const unsigned char *data, size_t length) {
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;
	RtpSession *session = context->mStreamSessions->rtp_session;
	RtpTransport *rtpt = NULL;
	mblk_t *msg;
	int ret;

	ms_message("DTLS Send RTP packet len %d sessions: %p rtp session %p", (int)length, context->mStreamSessions,
	           context->mStreamSessions->rtp_session);

	/* get RTP transport from session */
	rtp_session_get_transports(session, &rtpt, NULL);

	/* generate message from raw data */
	msg = rtp_create_packet((uint8_t *)data, length);

	ret = meta_rtp_transport_modifier_inject_packet_to_send(rtpt, context->rtp_modifier, msg, 0);
	freemsg(msg);

	/* sending failed - allow to retry at the next schedule tick */
	if (ret < 0) {
		ms_warning("DTLS Send RTP packet len %d sessions: %p rtp session %p failed returns %d", (int)length,
		           context->mStreamSessions, context->mStreamSessions->rtp_session, ret);
		context->retry_sending = true;
		return BCTBX_ERROR_NET_WANT_WRITE;
	}
	return ret;
}

int ms_dtls_srtp_rtp_DTLSread(void *ctx, unsigned char *buf, size_t len) {
	MSDtlsSrtpContext *context = (MSDtlsSrtpContext *)ctx;

	/* do we have something in the incoming buffer */
	if (context->mRtpIncomingBuffer.empty()) {
		return BCTBX_ERROR_NET_WANT_READ;
	} else { /* read the first packet in the buffer */
		auto packet = context->mRtpIncomingBuffer.front();
		auto packetSize = packet.size();
		if (packet.size() > len) {
			ms_error("DTLS wants to read incoming packet of size %d but provides a buffer size %d to read it",
			         (int)(packetSize), (int)(len));
			return BCTBX_ERROR_NET_WANT_READ;
		}
		memcpy(buf, packet.data(), packetSize);
		context->mRtpIncomingBuffer.pop(); // remove the packet from incoming buffer
		return (int)packetSize;
	}
}

/*******************************************************/
/**** Transport Modifier Sender/Receiver functions  ****/

int ms_dtls_srtp_rtp_process_on_receive(struct _RtpTransportModifier *t, mblk_t *msg) {
	MSDtlsSrtpContext *ctx = (MSDtlsSrtpContext *)t->data;

	size_t msgLength = msgdsize(msg);

	/* check if we have an on-going handshake */
	if (ctx->mChannelStatus == DtlsStatus::ContextNotReady) {
		return (int)msgLength;
	}

	// check incoming message length
	if (msgLength < RTP_FIXED_HEADER_SIZE) {
		return (int)msgLength;
	}

	/* check if it is a DTLS packet (first byte B as 19 < B < 64) rfc5764 section 5.1.2 */
	if (!((*(msg->b_rptr) > 19) && (*(msg->b_rptr) < 64))) {
		return (int)msgLength;
	}

	std::lock_guard<std::mutex> lock(ctx->mtx);
	/* process it */
	int ret = ctx->processDtlsPacket(msg);
	if ((ret == 0) &&
	    (ctx->mChannelStatus ==
	     DtlsStatus::HandshakeOngoing)) { /* handshake is over, give the keys to srtp : 128 bits client write - 128
		                                      bits server write - 112 bits client salt - 112 server salt */
		ctx->mChannelStatus = DtlsStatus::HandshakeOver;

		/* check the srtp profile get selected during handshake */
		ctx->mSrtpProtectionProfile = ms_dtls_srtp_bctbx_protection_profile_to_ms_crypto_suite(
		    bctbx_ssl_get_dtls_srtp_protection_profile(ctx->mDtlsCryptoContext.ssl));
		if (ctx->mSrtpProtectionProfile == MS_CRYPTO_SUITE_INVALID) {
			ms_message("DTLS RTP handshake successful but unable to agree on srtp_profile to use");
			return 0;
		} else {
			/* Get key material generated by DTLS handshake */
			size_t dtls_srtp_key_material_length = ctx->mSrtpKeyMaterial.size();
			ms_message("DTLS Handshake on RTP channel successful, srtp protection profile %d",
			           ctx->mSrtpProtectionProfile);

			ctx->rtp_time_reference = 0; /* unarm the timer */
			ret = bctbx_ssl_get_dtls_srtp_key_material(ctx->mDtlsCryptoContext.ssl, ctx->mSrtpKeyMaterial.data(),
			                                           &dtls_srtp_key_material_length);
			if (ret < 0) {
				ms_error("DTLS RTP Handshake : Unable to retrieve DTLS SRTP key material [-0x%x]", -ret);
				return 0;
			}

			/* Check certificate fingerprint */
			if (ctx->mPeerFingerprint.empty()) { /* fingerprint not set yet - peer's 200Ok didn't arrived yet */
				ms_warning("DTLS-SRTP: RTP empty peer fingerprint - waiting for it");
				return 0;
			}

			if (ms_dtls_srtp_check_certificate_fingerprint(bctbx_ssl_get_peer_certificate(ctx->mDtlsCryptoContext.ssl),
			                                               ctx->mPeerFingerprint) == 1) {
				ctx->setKeyMaterial();
				ctx->mChannelStatus = DtlsStatus::FingerprintVerified;
				ctx->checkChannelStatus();
			}
		}

		if (ctx->mRole != MSDtlsSrtpRoleIsServer) { /* close the connection only if we are client, if we are server,
			                                          the client may ask again for last packets */
			/*FireFox version 43 requires DTLS channel to be kept openned, probably a bug in FireFox ret =
			 * ssl_close_notify( &(ctx->mDtlsCryptoContext.ssl) );*/
		}
	}
	return 0;
}

int ms_dtls_srtp_rtp_process_on_send(BCTBX_UNUSED(struct _RtpTransportModifier *t), mblk_t *msg) {
	return (int)msgdsize(msg);
}

/**************************************/
/**** session management functions ****/
void ms_dtls_srtp_transport_modifier_destroy(RtpTransportModifier *tp) {
	ms_free(tp);
}

int ms_dtls_srtp_transport_modifier_new(MSDtlsSrtpContext *ctx, RtpTransportModifier **rtpt) {
	if (rtpt) {
		*rtpt = ms_new0(RtpTransportModifier, 1);
		(*rtpt)->data = ctx; /* back link to get access to the other fields of the MSDtlsSrtpContext from the
		                        RtpTransportModifier structure */
		(*rtpt)->t_process_on_send = ms_dtls_srtp_rtp_process_on_send;
		(*rtpt)->t_process_on_receive = ms_dtls_srtp_rtp_process_on_receive;
		(*rtpt)->t_process_on_schedule = schedule_rtp;
		(*rtpt)->t_destroy = ms_dtls_srtp_transport_modifier_destroy;
	}
	return 0;
}

void ms_dtls_srtp_set_transport(MSDtlsSrtpContext *ctx, RtpSession *s) {
	RtpTransport *rtpt = NULL;
	RtpTransportModifier *rtp_modifier;

	rtp_session_get_transports(s, &rtpt, NULL);
	ms_dtls_srtp_transport_modifier_new(ctx, &rtp_modifier);
	meta_rtp_transport_append_modifier(rtpt, rtp_modifier);
	/* save transport modifier into context, needed to inject packets generated by DTLS */
	ctx->rtp_modifier = rtp_modifier;
}
} // anonymous namespace

/***********************************************/
/***** MSDtlsSrtpContext methods           *****/
/***********************************************/
int MSDtlsSrtpContext::initialiseDtlsCryptoContext(MSDtlsSrtpParams *params) {
	int ret;
	bctbx_dtls_srtp_profile_t dtls_srtp_protection_profiles[2] = {BCTBX_SRTP_AES128_CM_HMAC_SHA1_80,
	                                                              BCTBX_SRTP_AES128_CM_HMAC_SHA1_32};

	/* initialise certificate */
	ret = bctbx_x509_certificate_parse(mDtlsCryptoContext.crt, (const char *)params->pem_certificate,
	                                   strlen(params->pem_certificate) + 1);
	if (ret < 0) {
		return ret;
	}

	ret = bctbx_signing_key_parse(mDtlsCryptoContext.pkey, (const char *)params->pem_pkey, strlen(params->pem_pkey) + 1,
	                              NULL, 0);
	if (ret != 0) {
		return ret;
	}

	/* configure ssl */
	if (params->role == MSDtlsSrtpRoleIsClient) {
		bctbx_ssl_config_defaults(mDtlsCryptoContext.ssl_config, BCTBX_SSL_IS_CLIENT, BCTBX_SSL_TRANSPORT_DATAGRAM);
	} else { /* configure it by default as server, nothing is actually performed until we start the channel but this
		        helps to get correct defaults settings */
		bctbx_ssl_config_defaults(mDtlsCryptoContext.ssl_config, BCTBX_SSL_IS_SERVER, BCTBX_SSL_TRANSPORT_DATAGRAM);
	}

	bctbx_ssl_config_set_dtls_srtp_protection_profiles(
	    mDtlsCryptoContext.ssl_config, dtls_srtp_protection_profiles,
	    2); /* TODO: get param from caller to select available profiles */

	bctbx_ssl_config_set_rng(mDtlsCryptoContext.ssl_config, (int (*)(void *, unsigned char *, size_t))bctbx_rng_get,
	                         mDtlsCryptoContext.rng);

	/* set certificates */
	/* this will force server to send his certificate to client as we need it to compute the fingerprint even if we
	 * won't verify it */
	bctbx_ssl_config_set_authmode(mDtlsCryptoContext.ssl_config, BCTBX_SSL_VERIFY_OPTIONAL);
	bctbx_ssl_config_set_own_cert(mDtlsCryptoContext.ssl_config, mDtlsCryptoContext.crt, mDtlsCryptoContext.pkey);
	/* This is useless as peer would certainly be a self signed certificate and we won't verify it but avoid runtime
	 * warnings */
	bctbx_ssl_config_set_ca_chain(mDtlsCryptoContext.ssl_config, mDtlsCryptoContext.crt);

	/* we are not ready yet to actually start the ssl context, this will be done by calling bctbx_ssl_context_setup when
	 * stream starts */
	return 0;
}

void MSDtlsSrtpContext::setRole(MSDtlsSrtpRole role) {
	/* if role has changed and handshake already setup and going, reset the session */
	if (mRole != role) {
		if ((mChannelStatus == DtlsStatus::HandshakeOngoing) || (mChannelStatus == DtlsStatus::HandshakeOver)) {
			bctbx_ssl_session_reset(mDtlsCryptoContext.ssl);
		}
	} else {
		return;
	}

	/* if role is server and was Unset or Client, we must complete the server setup */
	if (((mRole == MSDtlsSrtpRoleIsClient) || (mRole == MSDtlsSrtpRoleUnset)) && (role == MSDtlsSrtpRoleIsServer)) {
		bctbx_ssl_config_set_endpoint(mDtlsCryptoContext.ssl_config, BCTBX_SSL_IS_SERVER);
	}
	ms_message("DTLS set role from [%s] to [%s] for context [%p]",
	           mRole == MSDtlsSrtpRoleIsServer ? "server" : (mRole == MSDtlsSrtpRoleIsClient ? "client" : "unset role"),
	           role == MSDtlsSrtpRoleIsServer ? "server" : (role == MSDtlsSrtpRoleIsClient ? "client" : "unset role"),
	           this);
	mRole = role;
}

void MSDtlsSrtpContext::createSslContext() {
	ms_message("DTLS create a new ssl context on stream session %p", mStreamSessions);
	/* create the ssl context for this connection */
	if (mDtlsCryptoContext.ssl != NULL) {
		bctbx_ssl_context_free(mDtlsCryptoContext.ssl);
	}
	mDtlsCryptoContext.ssl = bctbx_ssl_context_new();
}

void MSDtlsSrtpContext::start() {
	ms_message("DTLS start stream on stream sessions [%p], RTCP mux is %s, MTU is %d, role is %s", mStreamSessions,
	           rtp_session_rtcp_mux_enabled(mStreamSessions->rtp_session) ? "enabled" : "disabled", mtu,
	           mRole == MSDtlsSrtpRoleIsServer ? "server"
	                                           : (mRole == MSDtlsSrtpRoleIsClient ? "client" : "unset role"));

	/* if we are client, start the handshake(send a clientHello) */
	if (mRole == MSDtlsSrtpRoleIsClient) {
		createSslContext();
		bctbx_ssl_config_set_endpoint(mDtlsCryptoContext.ssl_config, BCTBX_SSL_IS_CLIENT);
		/* complete ssl setup*/
		bctbx_ssl_context_setup(mDtlsCryptoContext.ssl, mDtlsCryptoContext.ssl_config);
		bctbx_ssl_set_io_callbacks(mDtlsCryptoContext.ssl, this, ms_dtls_srtp_rtp_sendData, ms_dtls_srtp_rtp_DTLSread);
		bctbx_ssl_set_mtu(mDtlsCryptoContext.ssl, mtu);
		/* and start the handshake */
		bctbx_ssl_handshake(mDtlsCryptoContext.ssl);
		rtp_time_reference = bctbx_get_cur_time_ms(); /* arm the timer for retransmission */
		mChannelStatus = DtlsStatus::HandshakeOngoing;
	}

	/* if we are server and we didn't started yet the DTLS engine, do it now */
	if (mRole == MSDtlsSrtpRoleIsServer) {
		if (mChannelStatus == DtlsStatus::ContextReady) {
			createSslContext();
			bctbx_ssl_config_set_endpoint(mDtlsCryptoContext.ssl_config, BCTBX_SSL_IS_SERVER);
			/* complete ssl setup*/
			bctbx_ssl_context_setup(mDtlsCryptoContext.ssl, mDtlsCryptoContext.ssl_config);
			bctbx_ssl_set_io_callbacks(mDtlsCryptoContext.ssl, this, ms_dtls_srtp_rtp_sendData,
			                           ms_dtls_srtp_rtp_DTLSread);
			bctbx_ssl_set_mtu(mDtlsCryptoContext.ssl, mtu);
			mChannelStatus = DtlsStatus::HandshakeOngoing;
		}
	}
}

void MSDtlsSrtpContext::checkChannelStatus() {
	/* Check if we're are ready: rtp_channel done and rtcp mux on */
	if ((mChannelStatus == DtlsStatus::FingerprintVerified) &&
	    (rtp_session_rtcp_mux_enabled(mStreamSessions->rtp_session))) {
		OrtpEventData *eventData;
		OrtpEvent *ev;
		ev = ortp_event_new(ORTP_EVENT_DTLS_ENCRYPTION_CHANGED);
		eventData = ortp_event_get_data(ev);
		eventData->info.dtls_stream_encrypted = 1;
		rtp_session_dispatch_event(mStreamSessions->rtp_session, ev);
		ms_message("DTLS Event dispatched to all: secrets are on for this stream");
	}
}

/* Get keys from context and set then in the SRTP context according to the given stream type
 */
void MSDtlsSrtpContext::setKeyMaterial() {

	uint8_t key[256];
	uint8_t *key_material = mSrtpKeyMaterial.data();

	if (mRole == MSDtlsSrtpRoleIsServer) {
		/* reception(client write) key and salt +16bits padding */
		memcpy(key, key_material, DtlsSrtpKeyLen);
		memcpy(key + DtlsSrtpKeyLen, key_material + 2 * DtlsSrtpKeyLen, DtlsSrtpSaltLen);
		ms_media_stream_sessions_set_srtp_recv_key(mStreamSessions, mSrtpProtectionProfile, key,
		                                           DtlsSrtpKeyLen + DtlsSrtpSaltLen, MSSrtpKeySourceDTLS);
		/* emission(server write) key and salt +16bits padding */
		memcpy(key, key_material + DtlsSrtpKeyLen, DtlsSrtpKeyLen);
		memcpy(key + DtlsSrtpKeyLen, key_material + 2 * DtlsSrtpKeyLen + DtlsSrtpSaltLen, DtlsSrtpSaltLen);
		ms_media_stream_sessions_set_srtp_send_key(mStreamSessions, mSrtpProtectionProfile, key,
		                                           DtlsSrtpKeyLen + DtlsSrtpSaltLen, MSSrtpKeySourceDTLS);
	} else if (mRole == MSDtlsSrtpRoleIsClient) { /* this enpoint act as DTLS client */
		/* emission(client write) key and salt +16bits padding */
		memcpy(key, key_material, DtlsSrtpKeyLen);
		memcpy(key + DtlsSrtpKeyLen, key_material + 2 * DtlsSrtpKeyLen, DtlsSrtpSaltLen);
		ms_media_stream_sessions_set_srtp_send_key(mStreamSessions, mSrtpProtectionProfile, key,
		                                           DtlsSrtpKeyLen + DtlsSrtpSaltLen, MSSrtpKeySourceDTLS);
		/* reception(server write) key and salt +16bits padding */
		memcpy(key, key_material + DtlsSrtpKeyLen, DtlsSrtpKeyLen);
		memcpy(key + DtlsSrtpKeyLen, key_material + 2 * DtlsSrtpKeyLen + DtlsSrtpSaltLen, DtlsSrtpSaltLen);
		ms_media_stream_sessions_set_srtp_recv_key(mStreamSessions, mSrtpProtectionProfile, key,
		                                           DtlsSrtpKeyLen + DtlsSrtpSaltLen, MSSrtpKeySourceDTLS);
	}
}

/**
 * Check if the incoming message is a DTLS packet.
 * If it is, store it in the context incoming buffer and call the bctoolbox function wich will process it.
 * This function also manages the client retransmission timer
 *
 * @param[in] 		msg	the incoming message
 * @return	the value returned by the bctoolbox function processing the packet(ssl_handshake)
 */
int MSDtlsSrtpContext::processDtlsPacket(mblk_t *msg) {
	size_t msgLength = msgdsize(msg);
	bctbx_ssl_context_t *ssl = mDtlsCryptoContext.ssl;
	int ret = 0;

	/* UGLY PATCH CLIENT HELLO PACKET PARSING */
	const int Content_Type_Index = 0;
	const int Content_Length_Index = 11;
	const int Handshake_Type_Index = 13;
	const int Handshake_Message_Length_Index = 14;
	const int Handshake_Message_Seq_Index = 17;
	const int Handshake_Frag_Offset_Index = 19;
	const int Handshake_Frag_Length_Index = 22;
	const size_t Handshake_Header_Length = 25;
	unsigned char *frag = msg->b_rptr;
	size_t base_index = 0;
	int message_length = 0;
	int message_seq = 0;
	int current_message_seq = -1;
	int frag_offset = 0;
	int frag_length = 0;
	unsigned char *reassembled_packet = NULL;
	/* end of UGLY PATCH CLIENT HELLO PACKET PARSING */

	/*required by webrtc in server case when ice is not completed yet*/
	/* no more required because change is performed by ice.c once a check list is ready
	 * rtp_session_update_remote_sock_addr(rtp_session, msg,is_rtp,FALSE);*/

	ms_message("DTLS Receive RTP packet len %d sessions: %p rtp session %p", (int)msgLength, mStreamSessions,
	           mStreamSessions->rtp_session);

	/* UGLY PATCH CLIENT HELLO PACKET PARSING */
	/* Parse the DTLS packet to check if we have a Client Hello packet fragmented at DTLS level but all set in one
	 * datagram (some kind of bug in certain versions openssl produce that and mbedtls does not support Client Hello
	 * fragmentation ) This patch is not very resistant to any change and target that very particular situation, a
	 * better solution should be to implement support of Client Hello Fragmentation in mbedtls */
	if (msgLength > Handshake_Header_Length && frag[Content_Type_Index] == 0x16 &&
	    frag[Handshake_Type_Index] ==
	        0x01) { // If the first fragment(there may be only one) is of a DTLS Handshake Client Hello message
		while (base_index + Handshake_Header_Length <
		       msgLength) { // loop on the message, parsing all fragments it may contain (loop until we have at
			                // least enough unparsed byte to read a handshake header)
			if (frag[Content_Type_Index] == 0x16) {       // Type index 0x16 is DLTS Handshake message
				if (frag[Handshake_Type_Index] == 0x01) { // Handshake type 0x01 is Client Hello
					// Get message length
					message_length = frag[Handshake_Message_Length_Index] << 16 |
					                 frag[Handshake_Message_Length_Index + 1] << 8 |
					                 frag[Handshake_Message_Length_Index + 2];

					// message sequence number
					message_seq = frag[Handshake_Message_Seq_Index] << 8 | frag[Handshake_Message_Seq_Index + 1];
					if (current_message_seq == -1) {
						current_message_seq = message_seq;
					}

					// fragment offset
					frag_offset = frag[Handshake_Frag_Offset_Index] << 16 | frag[Handshake_Frag_Offset_Index + 1] << 8 |
					              frag[Handshake_Frag_Offset_Index + 2];

					// and fragment length
					frag_length = frag[Handshake_Frag_Length_Index] << 16 | frag[Handshake_Frag_Length_Index + 1] << 8 |
					              frag[Handshake_Frag_Length_Index + 2];

					// check the message is not malformed and would lead us to read after the message buffer
					// or write after our reassembled packet buffer
					if (base_index + Handshake_Header_Length + frag_length <=
					        msgLength // we will read frag_length starting at base_index (frag in the code)+ header
					    && frag_length + frag_offset <=
					           message_length) { // we will write in the reassembled buffer frag_length byte,
						                         // starting at frag_offset, the buffer is message_length long

						// If message length and fragment length differs, we have a fragmented Client Hello
						// Check they are part of the same message (message_seq)
						// We will just collect all fragments (in our very particuliar case, they are all in the
						// same datagram so we do not need long term storage, juste parsing this packet)
						if (message_length != frag_length && message_seq == current_message_seq) {
							if (reassembled_packet == NULL) { // this is first fragment we get
								reassembled_packet =
								    (unsigned char *)ms_malloc(Handshake_Header_Length + message_length);
								// copy the header
								memcpy(reassembled_packet, msg->b_rptr, Handshake_Header_Length);
								// set the message length to be in line with reassembled fragments
								reassembled_packet[Content_Length_Index] = ((message_length + 12) >> 8) & 0xFF;
								reassembled_packet[Content_Length_Index + 1] = (message_length + 12) & 0xFF;

								// set the frag length to be the same than message length
								reassembled_packet[Handshake_Frag_Length_Index] =
								    reassembled_packet[Handshake_Message_Length_Index];
								reassembled_packet[Handshake_Frag_Length_Index + 1] =
								    reassembled_packet[Handshake_Message_Length_Index + 1];
								reassembled_packet[Handshake_Frag_Length_Index + 2] =
								    reassembled_packet[Handshake_Message_Length_Index + 2];
							}
							// copy the received fragment
							memcpy(reassembled_packet + Handshake_Header_Length + frag_offset,
							       frag + Handshake_Header_Length, frag_length);
						}

						// read what is next in the datagram
						base_index += Handshake_Header_Length + frag_length; // bytes parsed so far
						frag += Handshake_Header_Length + frag_length; // point to the begining of the next fragment
					} else {                                           // message is malformed in a nasty way
						ms_warning("DTLS Received RTP packet len %d sessions: %p rtp session %p is malformed in an "
						           "agressive way",
						           (int)msgLength, mStreamSessions, mStreamSessions->rtp_session);
						base_index = msgLength; // get out of the while
						ms_free(reassembled_packet);
						reassembled_packet = NULL;
					}
				} else {
					base_index = msgLength; // get out of the while
					ms_free(reassembled_packet);
					reassembled_packet = NULL;
				}
			}
		}
	}
	/* end of UGLY PATCH CLIENT HELLO PACKET PARSING */
	// if we made a reassembled client hello packet, use this one as incoming dlts packet
	if (reassembled_packet != NULL) {
		ms_message("DTLS re-assembled a fragmented Client Hello packet");
		mRtpIncomingBuffer.emplace(reassembled_packet, reassembled_packet + Handshake_Header_Length + message_length);
		ms_free(reassembled_packet);
	} else { // otherwise enqueue the one we received
		mRtpIncomingBuffer.emplace(msg->b_rptr, msg->b_rptr + msgLength);
	}

	/* while DTLS handshake is on going route DTLS packets to bctoolbox engine through ssl_handshake() */
	if (mChannelStatus < DtlsStatus::HandshakeOver) {
		/* role is unset but we receive a packet: we are caller and shall initialise as server and then process the
		 * incoming packet */
		if (mRole == MSDtlsSrtpRoleUnset) {
			setRole(MSDtlsSrtpRoleIsServer); /* this call will update role and complete server setup */
			start();                         /* complete the ssl setup and change channel_status to handshake ongoing */
			ssl = mDtlsCryptoContext.ssl;
		}
		/* process the packet and store result */
		ret = bctbx_ssl_handshake(ssl);
		ms_message("DTLS Handshake process RTP packet len %d sessions: %p rtp session %p return %s0x%0x",
		           (int)msgLength, mStreamSessions, mStreamSessions->rtp_session, ret > 0 ? "+" : "-",
		           ret > 0 ? ret : -ret);

		/* if we are client, manage the retransmission timer */
		if (mRole == MSDtlsSrtpRoleIsClient) {
			rtp_time_reference = bctbx_get_cur_time_ms();
		}
	} else { /* when DTLS handshake is over, route DTLS packets to bctoolbox engine through ssl_read() */
		/* we need a buffer to store the message read even if we don't use it */
		unsigned char *buf = (unsigned char *)ms_malloc(msgLength + 1);
		ret = bctbx_ssl_read(ssl, buf, msgLength);
		ms_message("DTLS Handshake read RTP packet len %d sessions: %p rtp session %p return %s0x%0x", (int)msgLength,
		           mStreamSessions, mStreamSessions->rtp_session, ret > 0 ? "+" : "-", ret > 0 ? ret : -ret);
		ms_free(buf);
	}

	/* report the error in logs only when different than requested read(waiting for data) */
	if (ret < 0 && ret != BCTBX_ERROR_NET_WANT_READ) {
		char err_str[512];
		err_str[0] = '\0';
		bctbx_strerror(ret, err_str, 512);
		ms_warning("DTLS Handshake returns -0x%x : %s [on sessions: %p rtp session %p]", -ret, err_str, mStreamSessions,
		           mStreamSessions->rtp_session);
	}
	return ret;
}

/***********************************************/
/***** EXPORTED FUNCTIONS                  *****/
/***********************************************/
/**** Private to mediastreamer2 functions ****/
/* header declared in voip/private.h */
extern "C" void ms_dtls_srtp_set_stream_sessions(MSDtlsSrtpContext *dtls_context,
                                                 MSMediaStreamSessions *stream_sessions) {
	if (dtls_context != NULL) {
		dtls_context->mStreamSessions = stream_sessions;
	}
}

/**** Public functions ****/
/* header declared in include/mediastreamer2/dtls_srtp.h */
extern "C" bool_t ms_dtls_srtp_available() {
	return ms_srtp_supported() && bctbx_dtls_srtp_supported();
}

extern "C" void ms_dtls_srtp_set_peer_fingerprint(MSDtlsSrtpContext *ctx, const char *peer_fingerprint) {
	if (ctx) {
		std::lock_guard<std::mutex> lock(ctx->mtx);

		if (strlen(peer_fingerprint) > maxFingerPrintSize) {
			ms_error("DTLS-SRTP received from SDP INVITE a peer fingerprint %d bytes length wich is longer than "
			         "maximum allowed size: %d bytes, ignore it",
			         (int)(strlen(peer_fingerprint)), (int)maxFingerPrintSize);
		} else {
			ctx->mPeerFingerprint = peer_fingerprint;
		}
		ms_message("DTLS-SRTP peer fingerprint is %s", ctx->mPeerFingerprint.c_str());

		/* Check if any of the context has finished its handshake and was waiting for the fingerprint */
		if (ctx->mChannelStatus == DtlsStatus::HandshakeOver) {
			ms_message("DTLS SRTP : late fingerprint arrival, check it after RTP Handshake is over");
			if (ms_dtls_srtp_check_certificate_fingerprint(bctbx_ssl_get_peer_certificate(ctx->mDtlsCryptoContext.ssl),
			                                               ctx->mPeerFingerprint) == 1) {
				ctx->setKeyMaterial();
				ctx->mChannelStatus = DtlsStatus::FingerprintVerified;
				ctx->checkChannelStatus();
			}
		}
	}
}

extern "C" void ms_dtls_srtp_reset_context(MSDtlsSrtpContext *context) {
	if (context) {
		std::lock_guard<std::mutex> lock(context->mtx);

		ms_message("Reseting DTLS context [%p] and SSL connections", context);

		if ((context->mChannelStatus == DtlsStatus::HandshakeOngoing) ||
		    (context->mChannelStatus == DtlsStatus::HandshakeOver)) {
			bctbx_ssl_session_reset(context->mDtlsCryptoContext.ssl);
		}

		context->mChannelStatus = DtlsStatus::ContextReady;
		context->mSrtpProtectionProfile = MS_CRYPTO_SUITE_INVALID;

		context->mRole = MSDtlsSrtpRoleUnset;
	}
}

extern "C" MSDtlsSrtpRole ms_dtls_srtp_get_role(const MSDtlsSrtpContext *context) {
	return context->mRole;
}

extern "C" void ms_dtls_srtp_set_role(MSDtlsSrtpContext *context, MSDtlsSrtpRole role) {
	if (context) {
		std::lock_guard<std::mutex> lock(context->mtx);
		context->setRole(role);
	}
}

extern "C" MSDtlsSrtpContext *ms_dtls_srtp_context_new(MSMediaStreamSessions *sessions, MSDtlsSrtpParams *params) {

	ms_message("Creating DTLS-SRTP engine on stream sessions [%p] as %s, RTCP mux is %s", sessions,
	           params->role == MSDtlsSrtpRoleIsServer
	               ? "server"
	               : (params->role == MSDtlsSrtpRoleIsClient ? "client" : "unset role"),
	           rtp_session_rtcp_mux_enabled(sessions->rtp_session) ? "enabled" : "disabled");

	/* create context and link it to the rtp session */
	auto context = new MSDtlsSrtpContext(sessions, params);
	ms_dtls_srtp_set_transport(context, sessions->rtp_session);

	int ret = context->initialiseDtlsCryptoContext(params);
	if (ret != 0) {
		ms_error("DTLS init error : rtp bctoolbox context init returned -0x%0x on stream session [%p]", -ret, sessions);
		delete context;
		return NULL;
	}

	context->mChannelStatus = DtlsStatus::ContextReady;
	return context;
}

extern "C" void ms_dtls_srtp_start(MSDtlsSrtpContext *context) {

	if (context == NULL) {
		ms_warning("DTLS start but no context\n");
		return;
	}
	std::lock_guard<std::mutex> lock(context->mtx);
	context->start();
}

extern "C" void ms_dtls_srtp_context_destroy(MSDtlsSrtpContext *ctx) {
	delete ctx;
	ms_message("DTLS-SRTP context destroyed");
}
