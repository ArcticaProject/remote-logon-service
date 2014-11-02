/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ted Gould <ted@canonical.com>
 */

#ifndef __DEFINES_H__
#define __DEFINES_H__

#define CONFIG_MAIN_GROUP "Remote Login Service"
#define CONFIG_MAIN_SERVERS   "Servers"
#define CONFIG_SERVER_PREFIX  "Server"
#define CONFIG_SERVER_NAME    "Name"
#define CONFIG_SERVER_URI     "URI"

#define CONFIG_UCCS_EXEC      "Exec"
#define CONFIG_UCCS_NETWORK   "NetworkRequired"
#define CONFIG_UCCS_NETWORK_NONE "None"
#define CONFIG_UCCS_NETWORK_GLOBAL "Global"
#define CONFIG_UCCS_VERIFY    "VerifyServer"

#define CONFIG_SERVER_TYPE       "Type"
#define CONFIG_SERVER_TYPE_RDP   "RDP"
#define CONFIG_SERVER_TYPE_ICA   "ICA"
#define CONFIG_SERVER_TYPE_X2GO  "X2GO"
#define CONFIG_SERVER_TYPE_UCCS  "UCCS"

#define JSON_PROTOCOL        "Protocol"
#define JSON_SERVER_NAME     "Name"
#define JSON_URI             "URL"
#define JSON_USERNAME        "Username"
#define JSON_PASSWORD        "Password"
#define JSON_DOMAIN_REQ      "DomainRequired"
#define JSON_DOMAIN          "WindowsDomain"
#define JSON_SESSIONTYPE     "SessionType"
#define JSON_SESSIONTYPE_REQ "SessionTypeRequired"

#endif /* __DEFINES_H__ */
