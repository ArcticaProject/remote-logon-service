# Remote Logon Service

## Introduction

A small service to grab various remote login possibilities from the local
network configuration, network servers that provide them, and present
them to the user to log into for both applications and full desktop
posibilities.

This project has been forked from Ubuntu's remote-login-service project
[1] by the X2Go/Arctica Project to support Remote Login against Linux
machines via X2Go.

## Configuring the Remote Logon Service

The service's configuration file is located in
```
  /etc/remote-logon-service.conf
```
Adapt the default configuration to your needs.

### Using Arctica Session Broker as service backend

This backend is not available, yet.

### Using an X2Go Session Broker as service backend

When using an X2Go Session Broker as service backend, adapt the service
configuration file like this:

```
[Remote Login Service]
Servers=MyUCCSTest

[Server MyUCCSTest]
Name=Remote Login
URI=http://x2gobroker.localdomain:8080/uccs/inifile/
```

## Testing the service

After you have installed and configured the Remote Logon Service, you can
query the remote logon service manually using the ´´dbus-send´´ command:

```
dbus-send --session --print-reply --dest="org.ArcticaProject.RemoteLogon" \
          /org/ArcticaProject/RemoteLogon org.ArcticaProject.RemoteLogon.GetServersForLogin \
          string:'http://x2gobroker.localdomain:8080/uccs/inifile/' \
          string:'<user>' \
          string:'<password>' \
          boolean:true
```

[1] https://launchpad.net/remote-login-service
