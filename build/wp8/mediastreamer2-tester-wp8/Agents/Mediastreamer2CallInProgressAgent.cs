/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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
﻿using Microsoft.Phone.Networking.Voip;
using System.Diagnostics;

namespace Mediastreamer2.Agents
{
    public class Mediastreamer2CallInProgressAgent : VoipCallInProgressAgent
    {
        public Mediastreamer2CallInProgressAgent() : base()
        {
        }

        /// <summary>
        /// Called when the first call has started.
        /// </summary>
        protected override void OnFirstCallStarting()
        {
            Debug.WriteLine("[Mediastreamer2CallInProgressAgent] The first call has started.");
        }

        /// <summary>
        /// Called when the last call has ended.
        /// </summary>
        protected override void OnCancel()
        {
            Debug.WriteLine("[Mediastreamer2CallInProgressAgent] The last call has ended. Calling NotifyComplete");
            base.NotifyComplete();
        }
    }
}