====
mgmt
====

-------------------------------------------
Bluetooth Management Protocol documentation
-------------------------------------------

:Version: BlueZ
:Copyright: Free use of this software is granted under the terms of the GNU
            Lesser General Public Licenses (LGPL).
:Date: April 2025
:Manual section: 7
:Manual group: Linux System Administration

SYNOPSIS
========

The Bluetooth management sockets can be created by setting the hci_channel
member of struct sockaddr_hci to HCI_CHANNEL_CONTROL (see hci(7)) when creating
a raw HCI socket::

    int mgmt_create(void)
    {
        struct sockaddr_hci addr;
        int fd;

        fd = socket(PF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK,
                                                                BTPROTO_HCI);
        if (fd < 0)
            return -errno;

        memset(&addr, 0, sizeof(addr));
        addr.hci_family = AF_BLUETOOTH;
        addr.hci_dev = HCI_DEV_NONE;
        addr.hci_channel = HCI_CHANNEL_CONTROL;

        if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            int err = -errno;
            close(fd);
            return err;
        }

        return fd;
   }

The process creating the mgmt socket is required to have the
CAP_NET_ADMIN capability (e.g. root would have this).

DESCRIPTION
===========

This document describes the format of data used for communicating with
the kernel using a so-called Bluetooth Management sockets. These sockets
are available starting with Linux kernel version 3.4

The following kernel versions introduced new commands, new events or
important fixes to the Bluetooth Management API:

.. csv-table::
	:header: "Version", "Kernel Version", "Commands", "Events"
	:widths: auto

	1.1, 3.4, Set Device ID,
	1.2, 3.7,,Passkey Notify
	1.3, 3.9,,
	1.4, 3.13, Set Advertising,
	,,Set BR/EDR,
	,,Set Static Address,
	,,Set Scan Parameters,
	1.5, 3.15, Set Secure Connections, New Identity Resolving Key
	,,Set Debug Keys, New Signature Resolving Key
	,,Set Privacy,
	,,Load Identity Resolving Keys,
	1.6, 3.16, Get Connection Information,
	1.7, 3.17, Get Clock Information,
	,,Add Device, Device Added
	,,Remove Device, Device Removed
	,,Load Connection Parameters, New Connection Parameter
	,,Read Unconfigured Index List, Unconfigured Index Added
	,,Read Controller Configuration Information, Unconfigured Index Removed
	,,Set External Configuration, New Configuration Options
	,,Set Public Address commands,
	1.8, 3.19, Start Service Discovery,
	1.9, 4.1, Read Local Out Of Band Extended Data, Extended Index Added
	,,Read Extended Controller Index List, Extended Index Removed
	,,Read Advertising Features, Local Out Of Band Extended Data Updated
	,,Add Advertising, Advertising Added
	,,Remove Advertising, Advertising Removed
	1.10, 4.2,,
	1.11, 4.5, Get Advertising Size Information,
	,,Start Limited Discovery,
	1.12, 4.6,,
	1.13, 4.8,,
	1.14, 4.9, Read Extended Controller Information, Extended Controller Information Changed
	,,Set Appearance,
	1.15, 5.5, Get PHY Configuration, PHY Configuration Changed
	,,Set PHY Configuration,
	,,Load Blocked Keys,
	1.16, 5.6, Set Wideband Speech,
	1.17, 5.7, Read Controller Capabilities, Experimental Feature Changed
	,,Read Experimental Features Information,
	,,Set Experimental Feature,
	1.18, 5.8, Read Default System Configuration, Default System Configuration Changed
	,,Set Default System Configuration, Default Runtime Configuration Changed
	,,Read Default Runtime Configuration,  Default Runtime Configuration Changed
	,,Set Default Runtime Configuration, Device Flags Changed
	,,Get Device Flags, Advertisement Monitor Added
	,,Set Device Flags, Advertisement Monitor Removed
	,,Read Advertisement Monitor Features, Controller Suspend
	,,Add Advertisement Patterns Monitor, Controller Resume
	,,Remove Advertisement Monitor, Advertisement Monitor Device Found
	,,,Advertisement Monitor Device Lost
	1.19, 5.10, Add Extended Advertising Parameters,
	,,Add Extended Advertising Data,
	1.20, 5.11, Add Advertisement Patterns Monitor With RSSI Threshold,
	1.21, 6.0, Set Mesh Receiver, Mesh Device Found
	,,Read Mesh Features, Mesh Packet Transmit Complete
	,,Transmit Mesh Packet,
	,,Cancel Transmit Mesh Packet,

Packet Structures
=================

Commands:

.. code-block::

    0    4    8   12   16   22   24   28   31   35   39   43   47
    +-------------------+-------------------+-------------------+
    |  Command Code     |  Controller Index |  Parameter Length |
    +-------------------+-------------------+-------------------+
    |                                                           |

Events:

.. code-block::

    0    4    8   12   16   22   24   28   31   35   39   43   47
    +-------------------+-------------------+-------------------+
    |  Event Code       |  Controller Index |  Parameter Length |
    +-------------------+-------------------+-------------------+
    |                                                           |

All fields are in little-endian byte order (least significant byte first).

Controller Index can have a special value <non-controller> to indicate that
command or event is not related to any controller. Possible values:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x0000-0xFFFE, <controller id>
    0xFFFF, <non-controller>

Error Codes
===========

The following values have been defined for use with the Command Status
and Command Complete events:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Success
    0x01, Unknown Command
    0x02, Not Connected
    0x03, Failed
    0x04, Connect Failed
    0x05, Authentication Failed
    0x06, Not Paired
    0x07, No Resources
    0x08, Timeout
    0x09, Already Connected
    0x0A, Busy
    0x0B, Rejected
    0x0C, Not Supported
    0x0D, Invalid Parameters
    0x0E, Disconnected
    0x0F,Not Powered
    0x10, Cancelled
    0x11, Invalid Index
    0x12, RFKilled
    0x13, Already Paired
    0x14, Permission Denied

As a general rule all commands generate the events as specified below,
however invalid lengths or unknown commands will always generate a
Command Status response (with Unknown Command or Invalid Parameters
status). Sending a command with an invalid Controller Index value will
also always generate a Command Status event with the Invalid Index
status code.

Commands
--------

Read Management Version Information
```````````````````````````````````

:Command Code:		0x0001
:Controller Index:	<non-controller>
:Command Parameters:
:Return Parameters:	Version (1 Octets)
:...:			Revision (2 Octets)

This command returns the Management version and revision.
Besides, being informational the information can be used to determine whether
certain behavior has changed or bugs fixed when interacting with the kernel.

This command generates a Command Complete event on success or a Command Status
event on failure.

Read Management Supported Commands
``````````````````````````````````

:Command Code:		0x0002
:Controller Index:	<non-controller>
:Command Parameters:
:Return Parameters:	Num_Of_Commands (2 Octets)
:...:			Num_Of_Events (2 Octets)
:...:			Command[] (2 Octets)
:...:			...[]
:...:			Event[] (2 Octets)
:...:			...[]

This command returns the list of supported Management commands and events.

The commands Read Management Version Information and Read management Supported
Commands are not included in this list.
Both commands are always supported and mandatory.

The events Command Status and Command Complete are not included in this list.
Both are implicit and mandatory.

This command generates a Command Complete event on success or a Command Status
event on failure.

Read Controller Index List
``````````````````````````

:Command Code:		0x0003
:Controller Index:	<non-controller>
:Command Parameters:
:Return Parameters:	Num_Controllers (2 Octets)
:...:			Controller_Index[] (2 Octets)

This command returns the list of currently known controllers.
Controllers added or removed after calling this command can be monitored using
the Index Added and Index Removed events.

This command generates a Command Complete event on success or a Command Status
event on failure.

Read Controller Information
```````````````````````````

:Command Code:		0x0004
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Address (6 Octets)
:...:			Bluetooth_Version (1 Octet)
:...:			Manufacturer (2 Octets)
:...:			Supported_Settings (4 Octets)
:...:			Current_Settings (4 Octets)
:...:			Class_Of_Device (3 Octets)
:...:			Name (249 Octets)
:...:			Short_Name (11 Octets)

This command is used to retrieve the current state and basic information of a
controller. It is typically used right after getting the response to the Read
Controller Index List command or an Index Added event.

The Address parameter describes the controllers public address and it can be
expected that it is set. However in case of single mode Low Energy only
controllers it can be 00:00:00:00:00:00. To power on the controller in this
case, it is required to configure a static address using Set Static Address
command first.

If the public address is set, then it will be used as identity address for the
controller. If no public address is available, then the configured static
address will be used as identity address.

In the case of a dual-mode controller with public address that is configured as
Low Energy only device (BR/EDR switched off), the static address is used when
set and public address otherwise.

If no short name is set the Short_Name parameter will be empty (begin with a nul
byte).

Current_Settings and Supported_Settings is a bitmask with currently the
following available bits:

.. csv-table::
    :header: "Bit", "Description"
    :widths: auto

    0, Powered
    1, Connectable
    2, Fast Connectable
    3, Discoverable
    4, Bondable
    5, Link Level Security (Sec. mode 3)
    6, Secure Simple Pairing
    7, Basic Rate/Enhanced Data Rate
    8, High Speed
    9, Low Energy
    10, Advertising
    11, Secure Connections
    12, Debug Keys
    13, Privacy
    14, Controller Configuration
    15, Static Address
    16, PHY Configuration
    17, Wideband Speech
    18, Connected Isochronous Stream - Central
    19, Connected Isochronous Stream - Peripheral
    20, Isochronous Broadcaster
    21, Synchronized Receiver
    22, LL Privacy

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Powered
```````````

:Command Code:		0x0005
:Controller Index:	<controller id>
:Command Parameters:	Powered (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to power on or off a controller. The allowed Powered
command parameter values are 0x00 and 0x01. All other values will return Invalid
Parameters.

If discoverable setting is activated with a timeout, then switching the
controller off will expire this timeout and disable discoverable.

Settings programmed via Set Advertising and Add/Remove Advertising while the
controller was powered off will be activated when powering the controller on.

Switching the controller off will permanently cancel and remove all advertising
instances with a timeout set, i.e. time limited advertising instances are not
being remembered across power cycles.
Advertising Removed events will be issued accordingly.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Invalid Parameters:
:Invalid Index:

Set Discoverable
````````````````

:Command Code:		0x0006
:Controller Index:	<controller id>
:Command Parameters:	Discoverable (1 Octet)
:...:			Timeout (2 Octets)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to set the discoverable property of a controller. The
allowed Discoverable command parameter values are:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Not Discoverable
    0x01, General Discoverable
    0x02 (since 1.4), Limited Discoverable

Timeout is the time in seconds and is only meaningful when Discoverable is set
to 0x01 or 0x02. Providing a timeout with 0x00 return Invalid Parameters. For
0x02, the timeout value is required.

This command is only available for BR/EDR capable controllers (e.g. not for
single-mode LE ones). It will return Not Supported otherwise.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

However using a timeout when the controller is not powered will return Not
Powered error.

When switching discoverable on and the connectable setting is off it will return
Rejected error.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Rejected:
:Not Supported:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Set Connectable
```````````````

:Command Code:		0x0007
:Controller Index:	<controller id>
:Command Parameters:	Connectable (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to set the connectable property of a controller. The
allowed Connectable command parameter values are 0x00 and 0x01. All other values
will return Invalid Parameters.

This command is available for BR/EDR, LE-only and also dual mode controllers.
For BR/EDR is changes the page scan setting and for LE controllers it changes
the advertising type. For dual mode controllers it affects both settings.

For LE capable controllers the connectable setting takes effect when advertising
is enabled (peripheral) or when directed advertising events are received
(central).

This command can be used when the controller is not powered and all settings
will be programmed once powered.

When switching connectable off, it will also switch off the discoverable
setting. Switching connectable back on will not restore a previous discoverable.
It will stay off and needs to be manually switched back on.

When switching connectable off, it will expire a discoverable setting with a
timeout.

This setting does not affect known devices from Add Device command. These
devices are always allowed to connect.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Fast Connectable
````````````````````

:Command Code:		0x0008
:Controller Index:	<controller id>
:Command Parameters:	Enable (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to set the controller into a connectable state where the
page scan parameters have been set in a way to favor faster connect times with
the expense of higher power consumption.

The allowed values of the Enable command parameter are 0x00 and 0x01. All other
values will return Invalid Parameters.

This command is only available for BR/EDR capable controllers (e.g. not for
single-mode LE ones). It will return Not Supported otherwise.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

The setting will be remembered during power down/up toggles.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Failed:
:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Bondable
````````````

:Command Code:		0x0009
:Controller Index:	<controller id>
:Command Parameters:	Bondable (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to set the bondable property of an controller. The allowed
values for the Bondable command parameter are 0x00 and 0x01. All other values
will return Invalid Parameters.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

Turning bondable on will not automatically switch the controller into
connectable mode. That needs to be done separately.

The setting will be remembered during power down/up toggles.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Link Security
`````````````````

:Command Code:		0x000A
:Controller Index:	<controller id>
:Command Parameters:	Link_Security (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to either enable or disable link level
security for an controller (also known as Security Mode 3). The allowed values
for the Link_Security command parameter are 0x00 and 0x01. All other values will
return Invalid Parameters.

This command is only available for BR/EDR capable controllers (e.g. not for
single-mode LE ones). It will return Not Supported otherwise.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Secure Simple Pairing
`````````````````````````

:Command Code:		0x000B
:Controller Index:	<controller id>
:Command Parameters:	Secure_Simple_Pairing (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable/disable Secure Simple Pairing support for a
controller. The allowed values for the Secure_Simple_Pairing command parameter
are 0x00 and 0x01. All other values will return Invalid Parameters.

This command is only available for BR/EDR capable controllers supporting the
core specification version 2.1 or greater (e.g. not for single-mode LE
controllers or pre-2.1 ones).

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller does not support Secure Simple Pairing, the command will
fail regardless with Not Supported error.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set High Speed
``````````````

:Command Code:		0x000C
:Controller Index:	<controller id>
:Command Parameters:	High_Speed (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable/disable Bluetooth High Speed support for a
controller. The allowed values for the High_Speed command parameter are 0x00 and
0x01. All other values will return Invalid Parameters.

This command is only available for BR/EDR capable controllers (e.g. not for
single-mode LE ones).

This command can be used when the controller is not powered and all settings
will be programmed once powered.

To enable High Speed support, it is required that Secure Simple Pairing support
is enabled first. High Speed support is not possible for connections without
Secure Simple Pairing.

When switching Secure Simple Pairing off, the support for High Speed will be
switched off as well. Switching Secure Simple Pairing back on, will not
re-enable High Speed support. That needs to be done manually.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Low Energy
``````````````

:Command Code:		0x000D
:Controller Index:	<controller id>
:Command Parameters:	Low_Energy (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable/disable Low Energy support for a controller. The
allowed values of the Low_Energy command parameter are 0x00 and 0x01. All other
values will return Invalid Parameters.

This command is only available for LE capable controllers and will yield in a
Not Supported error otherwise.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the kernel subsystem does not support Low Energy or the controller does
not either, the command will fail regardless.

Disabling LE support will permanently disable and remove all advertising
instances configured with the Add Advertising command. Advertising Removed
events will be issued accordingly.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Device Class
````````````````

:Command Code:		0x000E
:Controller Index:	<controller id>
:Command Parameters:	Major_Class (1 Octet)
:...:			Minor_Class (1 Octet)
:Return Parameters:	Class_Of_Device (3 Octets)

This command is used to set the major and minor device class for BR/EDR capable
controllers.

This command will also implicitly disable caching of pending CoD and EIR
updates.

This command is only available for BR/EDR capable controllers (e.g. not for
single-mode LE ones).

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller is powered off, 0x000000 will be returned for the class
of device parameter. And after power on the new value will be announced via
class of device changed event.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Local Name
``````````````

:Command Code:		0x000F
:Controller Index:	<controller id>
:Command Parameters:	Name (249 Octets)
:...:			Short_Name (11 Octets)
:Return Parameters:	Name (249 Octets)
:...:			Short_Name (11 Octets)

This command is used to set the local name of a controller. The command
parameters also include a short name which will be used in case the full name
doesn't fit within EIR/AD data.

The name parameters need to always end with a null byte (failure to do so will
cause the command to fail).

This command can be used when the controller is not powered and all settings
will be programmed once powered.

The values of name and short name will be remembered when switching the
controller off and back on again. So the name and short name only have to be set
once when a new controller is found and will stay until removed.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Add UUID
````````

:Command Code:		0x0010
:Controller Index:	<controller id>
:Command Parameters:	UUID (16 Octets)
:...:			SVC_Hint (1 Octet)
:Return Parameters:	Class_Of_Device (3 Octets)

This command is used to add a UUID to be published in EIR data.
The accompanied SVC_Hint parameter is used to tell the kernel whether the
service class bits of the Class of Device value need modifying due to this UUID.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller is powered off, 0x000000 will be returned for the class
of device parameter. And after power on the new value will be announced via
class of device changed event.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Invalid Parameters:
:Invalid Index:

Remove UUID
```````````

:Command Code:		0x0011
:Controller Index:	<controller id>
:Command Parameters:	UUID (16 Octets)
:Return Parameters:	Class_Of_Device (3 Octets)

This command is used to remove a UUID previously added using the Add UUID
command.

When the UUID parameter is an empty UUID (16 x 0x00), then all previously loaded
UUIDs will be removed.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller is powered off, 0x000000 will be returned for the class
of device parameter. And after power on the new value will be announced via
class of device changed event.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Invalid Parameters:
:Invalid Index:

Load Link Keys
``````````````

:Command Code:		0x0012
:Controller Index:	<controller id>
:Command Parameters:	Debug_Keys (1 Octet)
:...:			Key_Count (2 Octets)
:...:			Address[] (6 Octets)
:...:			Address_Type[] (1 Octet)
:...:			Key_Type[] (1 Octet)
:...:			Value[] (16 Octets)
:...:			PIN_Length[] (1 Octet)
:...:			...[]
:Return Parameters:

This command is used to feed the kernel with currently known link keys. The
command does not need to be called again upon the receipt of New Link Key events
since the kernel updates its list automatically.

The Debug_Keys parameter is used to tell the kernel whether to accept the usage
of debug keys or not. The allowed values for this parameter are 0x00 and 0x01.
All other values will return an Invalid Parameters response.

Usage of the Debug_Keys parameter is deprecated and has been replaced with the
Set Debug Keys command. When setting the Debug_Keys option via Load Link Keys
command it has the same affect as setting it via Set Debug Keys and applies to
all keys in the system.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, BR/EDR
	1, Reserved (not in use)
	2, Reserved (not in use)

Public and random LE addresses are not valid and will be rejected.

Currently defined Key_Type values are:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Combination key
	0x01, Local Unit key
	0x02, Remote Unit key
	0x03, Debug Combination key
	0x04, Unauthenticated Combination key from P-192
	0x05, Authenticated Combination key from P-192
	0x06, Changed Combination key
	0x07, Unauthenticated Combination key from P-256
	0x08, Authenticated Combination key from P-256

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Load Long Term Keys
```````````````````

:Command Code:		0x0013
:Controller Index:	<controller id>
:Command Parameters:	Key_Count (2 Octets)
:...:			Address[] (6 Octets)
:...:			Address_Type[] (1 Octet)
:...:			Key_Type[] (1 Octet)
:...:			Central[] (1 Octet)
:...:			Encryption_Size[] (1 Octet)
:...:			Encryption_Diversifier[] (2 Octets)
:...:			Random_Number[] (8 Octets)
:...:			Value[] (16 Octets)
:...:			...[]
:Return Parameters:

This command is used to feed the kernel with currently known (SMP) Long Term
Keys. The command does not need to be called again upon the receipt of New Long
Term Key events since the kernel updates its list automatically.

Possible values for the Address_Type parameter:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Reserved (not in use)
    0x01, LE Public
    0x02, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

Unresolvable random addresses and resolvable random addresses are not valid and
will be rejected.

Currently defined Key_Type values are:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Unauthenticated key
    0x01, Authenticated key

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Disconnect
``````````

:Command Code:		0x0014
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to force the disconnection of a currently
connected device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Busy:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Get Connections
```````````````

:Command Code:		0x0015
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Connection_Count (2 Octets)
:...:			Address[] (6 Octets)
:...:			Address_Type[] (1 Octet)
:...:			...[]

This command is used to retrieve a list of currently connected devices.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For devices using resolvable random addresses with a known identity resolving
key, the Address and Address_Type will contain the identity information.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Not Powered:
:Invalid Index:

PIN Code Reply
``````````````

:Command Code:		0x0016
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			PIN_Length (1 Octet)
:...:			PIN_Code (16 Octets)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to respond to a PIN Code request event.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

PIN Code Negative Reply
```````````````````````

:Command Code:		0x0017
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to return a negative response to a PIN Code Request event.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Set IO Capability
`````````````````

:Command Code:		0x0018
:Controller Index:	<controller id>
:Command Parameters:	IO_Capability (1 Octet)
:Return Parameters:

This command is used to set the IO Capability used for pairing.
The command accepts both SSP and SMP values.

Possible values for the IO_Capability parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, DisplayOnly
	0x01, DisplayYesNo
	0x02, KeyboardOnly
	0x03, NoInputNoOutput
	0x04, KeyboardDisplay

Passing a value 0x04 (KeyboardDisplay) will cause the kernel to convert it to
0x01 (DisplayYesNo) in the case of a BR/EDR connection (as KeyboardDisplay is
specific to SMP).

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Pair Device
```````````

:Command Code:		0x0019
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			IO_Capability (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to trigger pairing with a remote device.
The IO_Capability command parameter is used to temporarily (for this pairing
event only) override the global IO Capability (set using the Set IO Capability
command).

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

Possible values for the IO_Capability parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, DisplayOnly
	0x01, DisplayYesNo
	0x02, KeyboardOnly
	0x03, NoInputNoOutput
	0x04, KeyboardDisplay

Passing a value 0x04 (KeyboardDisplay) will cause the kernel to convert it to
0x01 (DisplayYesNo) in the case of a BR/EDR connection (as KeyboardDisplay is
specific to SMP).

The Address and Address_Type of the return parameters will return the identity
address if known. In case of resolvable random address given as command
parameters and the remote provides an identity resolving key, the return
parameters will provide the resolved address.

To allow tracking of which resolvable random address changed into which identity
address, the New Identity Resolving Key event will be sent before receiving
Command Complete event for this command.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Reject status is used when requested transport is not enabled.

Not Supported status is used if controller is not capable with requested
transport.

Possible errors:

:Rejected:
:Not Supported:
:Connect Failed:
:Busy:
:Invalid Parameters:
:Not Powered:
:Invalid Index:
:Already Paired:

Cancel Pair Device
``````````````````

:Command Code:		0x001A
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

The Address and Address_Type parameters should match what was given to a
preceding Pair Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Invalid Parameters:
:Not Powered:
:Invalid Index:

Unpair Device
`````````````

:Command Code:		0x001B
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Disconnect (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

Removes all keys associated with the remote device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The Disconnect parameter tells the kernel whether to forcefully disconnect any
existing connections to the device. It should in practice always be 1 except for
some special GAP qualification test-cases where a key removal without
disconnecting is needed.

When unpairing a device its link key, long term key and if provided identity
resolving key will be purged.

For devices using resolvable random addresses where the identity resolving key
was available, after this command they will now no longer be resolved. The
device will essentially become private again.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Paired:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

User Confirmation Reply
```````````````````````

:Command Code:		0x001C
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to respond to a User Confirmation Request event.

Possible values for the Address_Type parameter:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, BR/EDR
    0x01, LE Public
    0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

User Confirmation Negative Reply
````````````````````````````````

:Command Code:		0x001D
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to return a negative response to a User Confirmation
Request event.

Possible values for the Address_Type parameter:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, BR/EDR
    0x01, LE Public
    0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

User Passkey Reply
``````````````````

:Command Code:		0x001E
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Passkey (4 Octets)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to respond to a User Confirmation Passkey Request event.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	    :widths: auto

	    0x00, BR/EDR
	    0x01, LE Public
	    0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

User Passkey Negative Reply
```````````````````````````

:Command Code:		0x001F
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to return a negative response to a User Passkey Request
event.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Not Connected:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Read Local Out Of Band Data
```````````````````````````

:Command Code:		0x0020
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Hash_192 (16 Octets)
:...:			Randomizer_192 (16 Octets)
:...:			Hash_256 (16 Octets, Optional)
:...:			Randomizer_256 (16 Octets, Optional)

This command is used to read the local Out of Band data.

This command can only be used when the controller is powered.

If Secure Connections support is enabled, then this command will return P-192
versions of hash and randomizer as well as P-256 versions of both.

Values returned by this command become invalid when the controller is powered
down. After each power-cycle it is required to call this command again to get
updated values.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Not Supported:
:Busy:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Add Remote Out Of Band Data
```````````````````````````

:Command Code:		0x0021
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Hash_192 (16 Octets)
:...:			Randomizer_192 (16 Octets)
:...:			Hash_256 (16 Octets, Optional)
:...:			Randomizer_256 (16 Octets, Optional)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to provide Out of Band data for a remote device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

Provided Out Of Band data is persistent over power down/up toggles.

This command also accept optional P-256 versions of hash and randomizer. If they
are not provided, then they are set to zero value.

The P-256 versions of both can also be provided when the support for Secure
Connections is not enabled. However in that case they will never be used.

To only provide the P-256 versions of hash and randomizer, it is valid to leave
both P-192 fields as zero values. If Secure Connections is disabled, then of
course this is the same as not providing any data at all.

When providing data for remote LE devices, then the Hash_192 and Randomizer_192
fields are not used and shell be set to zero.

The Hash_256 and Randomizer_256 fields can be used for LE secure connections Out
Of Band data. If only LE secure connections data is provided the Hash_P192 and
Randomizer_P192 fields can be set to zero. Currently there is no support for
providing the Security Manager TK Value for LE legacy pairing.

If Secure Connections Only mode has been enabled, then providing Hash_P192 and
Randomizer_P192 is not allowed. They are required to be set to zero values.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Failed:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Remove Remote Out Of Band Data
``````````````````````````````

:Command Code:		0x0022
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to remove data added using the Add Remote Out Of Band Data
command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

When the Address parameter is 00:00:00:00:00:00, then all previously added data
will be removed.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Invalid Parameters:
:Not Powered:
:Invalid Index:

Start Discovery
```````````````

:Command Code:		0x0023
:Controller Index:	<controller id>
:Command Parameters:	Address_Type (1 Octet)
:Return Parameters:	Address_Type (1 Octet)

This command is used to start the process of discovering remote devices. A
Device Found event will be sent for each discovered device.

Possible values for the Address_Type parameter are a bit-wise or of the
following bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

By combining these e.g. the following values are possible:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x01, BR/EDR
	0x06, LE (public & random)
	0x07, BR/EDR/LE (interleaved discovery)

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Stop Discovery
``````````````

:Command Code:		0x0024
:Controller Index:	<controller id>
:Command Parameters:	Address_Type (1 Octet)
:Return Parameters:	Address_Type (1 Octet)

This command is used to stop the discovery process started using the Start
Discovery command.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Rejected:
:Invalid Parameters:
:Invalid Index:

Confirm Name
````````````

:Command Code:		0x0025
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Name_Known (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is only valid during device discovery and is expected for each
Device Found event with the Confirm Name flag set.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The Name_Known parameter should be set to 0x01 if user space knows the name for
the device and 0x00 if it doesn't. If set to 0x00 the kernel will perform a name
resolving procedure for the device in question.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Failed:
:Invalid Parameters:
:Invalid Index:

Block Device
````````````

:Command Code:		0x0026
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to add a device to the list of devices which should be
blocked from being connected to the local controller.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For Low Energy devices, the blocking of a device takes precedence over
auto-connection actions provided by Add Device. Blocked devices will not be
auto-connected or even reported when found during background scanning. If the
controller is connectable direct advertising from blocked devices will also be
ignored.

Connections created from advertising of the controller will be dropped if the
device is blocked.

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Failed:
:Invalid Parameters:
:Invalid Index:

Unblock Device
``````````````

:Command Code:		0x0027
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to remove a device from the list of blocked devices (where
it was added to using the Block Device command).

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

When the Address parameter is 00:00:00:00:00:00, then all previously blocked
devices will be unblocked.

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Device ID
`````````````

:Command Code:		0x0028
:Controller Index:	<controller id>
:Command Parameters:	Source (2 Octets)
:...:			Vendor (2 Octets)
:...:			Product (2 Octets)
:...:			Version (2 Octets)
:Return Parameters:

This command can be used when the controller is not powered and all settings
will be programmed once powered.

The Source parameter selects the organization that assigned the Vendor
parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x0000, Disable Device ID
	0x0001, Bluetooth SIG
	0x0002, USB Implementer's Forum

The information is put into the EIR data. If the controller does not support EIR
or if SSP is disabled, this command will still succeed. The information is
stored for later use and will survive toggling SSP on and off.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Advertising (since 1.4)
```````````````````````````

:Command Code:		0x0029
:Controller Index:	<controller id>
:Command Parameters:	Advertising (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable LE advertising on a controller that supports it.
The allowed values for the Advertising command parameter are:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Disable advertising
    0x01, Enables advertising
    0x02 (since 1.9), Enabled advertising in connectable mode

Using value 0x01 means that when connectable setting is disabled, the
advertising happens with undirected non-connectable advertising packets and a
non-resolvable random address is used. If connectable setting is enabled, then
undirected connectable advertising packets and the identity address or
resolvable private address are used.

LE Devices configured via Add Device command with Action 0x01 have no effect
when using Advertising value 0x01 since only the connectable setting is taken
into account.

To utilize undirected connectable advertising without changing the connectable
setting, the value 0x02 can be utilized. It makes the device connectable via LE
without the requirement for being connectable on BR/EDR (and/or LE).

The value 0x02 should be the preferred mode of operation when implementing
peripheral mode.

Using this command will temporarily deactivate any configuration made by the Add
Advertising command. This command takes precedence. Once a Set Advertising
command with value 0x00 is issued any previously made configurations via
Add/Remove Advertising, including such changes made while Set Advertising was
active, will be re-enabled.

A pre-requisite is that LE is already enabled, otherwise this command will
return a "rejected" response.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set BR/EDR (since 1.4)
``````````````````````

:Command Code:		0x002A
:Controller Index:	<controller id>
:Command Parameters:	BR/EDR (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable or disable BR/EDR support on a dual-mode
controller. The allowed values for the Advertising command parameter are 0x00
and 0x01. All other values will return Invalid Parameters.

A pre-requisite is that LE is already enabled, otherwise this command will
return a "rejected" response. Enabling BR/EDR can be done both when powered on
and powered off, however disabling it can only be done when powered off
(otherwise the command will again return "rejected"). Disabling BR/EDR will
automatically disable all other BR/EDR related settings.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Static Address (since 1.4)
``````````````````````````````

:Command Code:		0x002B
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:Return Parameters:	Current_Settings (4 Octets)

This command allows for setting the static random address. It is only supported
on controllers with LE support. The static random address is suppose to be valid
for the lifetime of the controller or at least until the next power cycle. To
ensure such behavior, setting of the address is limited to when the controller
is powered off.

The special BDADDR_ANY address (00:00:00:00:00:00) can be used to disable the
static address.

When a controller has a public address (which is required for all dual-mode
controllers), this address is not used. If a dual-mode controller is configured
as Low Energy only devices (BR/EDR has been switched off), then the static
address is used. Only when the controller information reports BDADDR_ANY
(00:00:00:00:00:00), it is required to configure a static address first.

If privacy mode is enabled and the controller is single mode LE only without a
public address, the static random address is used as identity address.

The Static Address flag from the current settings can also be used to determine
if the configured static address is in use or not.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Scan Parameters (since 1.4)
```````````````````````````````

:Command Code:		0x002C
:Controller Index:	<controller id>
:Command Parameters:	Interval (2 Octets)
			Window (2 Octets)
:Return Parameters:

This command allows for setting the Low Energy scan parameters used for
connection establishment and passive scanning. It is only supported on
controllers with LE support.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Secure Connections (since 1.5)
``````````````````````````````````

:Command Code:		0x002D
:Controller Index:	<controller id>
:Command Parameters:	Secure_Connections (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable/disable Secure Connections support for a
controller. The allowed values for the Secure_Connections command parameter are
0x00, 0x01 and 0x02. All other values will return Invalid Parameters.

The value 0x00 disables Secure Connections, the value 0x01 enables Secure
Connections and the value 0x02 enables Secure Connections Only mode.

This command is only available for LE capable controllers as well as controllers
supporting the core specification version 4.1 or greater.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller does not support Secure Connections the command will fail
regardless with Not Supported error.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Debug Keys (since 1.5)
``````````````````````````

:Command Code:		0x002E
:Controller Index:	<controller id>
:Command Parameters:	Debug_Keys (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to tell the kernel whether to accept the usage of debug
keys or not. The allowed values for this parameter are:

.. csv-table::
    :header: "Value", "Description"
    :widths: auto

    0x00, Discard keys on disconnect
    0x01, Discard keys on reboot
    0x02 (since 1.7), Discard keys on reboot (SSP debug mode)

With a value of 0x00 any generated debug key will be discarded as soon as the
connection terminates.

With a value of 0x01 generated debug keys will be kept and can be used for
future connections. However debug keys are always marked as non persistent and
should not be stored. This means a reboot or changing the value back to 0x00
will delete them.

With a value of 0x02 generated debug keys will be kept and can be used for
future connections. This has the same affect as with value 0x01. However in
addition this value will also enter the controller mode to generate debug keys
for each new pairing. Changing the value back to 0x01 or 0x00 will disable the
controller mode for generating debug keys.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Privacy (since 1.5)
```````````````````````

:Command Code:		0x002F
:Controller Index:	<controller id>
:Command Parameters:	Privacy (1 Octet)
:...:			Identity_Resolving_Key (16 Octets)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable Low Energy Privacy feature using resolvable
private addresses.

The value 0x00 disables privacy mode, the values 0x01 and 0x02 enable privacy
mode.

With value 0x01 the kernel will always use the privacy mode. This means
resolvable private address is used when the controller is discoverable and also
when pairing is initiated.

With value 0x02 the kernel will use a limited privacy mode with a resolvable
private address except when the controller is bondable and discoverable, in
which case the identity address is used.

Exposing the identity address when bondable and discoverable or during initiated
pairing can be a privacy issue. For dual-mode controllers this can be neglected
since its public address will be exposed over BR/EDR anyway. The benefit of
exposing the identity address for pairing purposes is that it makes matching up
devices with dual-mode topology during device discovery now possible.

If the privacy value 0x02 is used, then also the GATT database should expose the
Privacy Characteristic so that remote devices can determine if the privacy
feature is in use or not.

When the controller has a public address (mandatory for dual-mode controllers)
it is used as identity address. In case the controller is single mode LE only
without a public address, it is required to configure a static random address
first. The privacy mode can only be enabled when an identity address is
available.

The Identity_Resolving_Key is the local key assigned for the local resolvable
private address.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Load Identity Resolving Keys (since 1.5)
````````````````````````````````````````

:Command Code:		0x0030
:Controller Index:	<controller id>
:Command Parameters:	Key_Count (2 Octets)
:...:			Address[] (6 Octets)
:...:			Address_Type[] (1 Octet)
:...:			Value[] (16 Octets)
:...:			...[]
:Return Parameters:

This command is used to feed the kernel with currently known identity resolving
keys. The command does not need to be called again upon the receipt of New
Identity Resolving Key events since the kernel updates its list automatically.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Reserved (not in use)
	0x01, LE Public
	0x02, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

Unresolvable random addresses and resolvable random addresses are not valid and
will be rejected.

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Get Connection Information (since 1.6)
``````````````````````````````````````

:Command Code:		0x0031
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			RSSI (1 Octet)
:...:			TX_Power (1 Octet)
:...:			Max_TX_Power (1 Octet)

This command is used to get connection information.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Reserved (not in use)
	0x01, LE Public
	0x02, LE Random

TX_Power and Max_TX_Power can be set to 127 if values are invalid or unknown. A
value of 127 for RSSI indicates that it is not available.

This command generates a Command Complete event on success and on failure. In
case of failure only Address and Address_Type fields are valid and values of
remaining parameters are considered invalid and shall be ignored.

Possible errors:

:Not Connected:
:Not Powered:
:Invalid Parameters:
:Invalid Index:

Get Clock Information (since 1.7)
`````````````````````````````````

:Command Code:		0x0032
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Local_Clock (4 Octets)
:...:			Piconet_Clock (4 Octets)
:...:			Accuracy (2 Octets)

This command is used to get local and piconet clock information.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, Reserved (not in use)
	0x02, Reserved (not in use)

The Accuracy can be set to 0xffff which means the value is unknown.

If the Address is set to 00:00:00:00:00:00, then only the Local_Clock field has
a valid value. The Piconet_Clock and Accuracy fields are invalid and shall be
ignored.

This command generates a Command Complete event on success and on failure. In
case of failure only Address and Address_Type fields are valid and values of
remaining parameters are considered invalid and shall be ignored.

Possible errors:

:Not Connected:
:Not Powered:
:Invalid Parameters:
:Invalid Index:

Add Device (since 1.7)
``````````````````````

:Command Code:		0x0033
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Action (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to add a device to the action list. The action list allows
scanning for devices and enables incoming connections from known devices.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

Possible values for the Action parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Background scan for device
	0x01, Allow incoming connection
	0x02, Auto-connect remote device

With the Action 0x00, when the device is found, a new Device Found event will be
sent indicating this device is available. This action is only valid for LE
Public and LE Random address types.

With the Action 0x01, the device is allowed to connect. For BR/EDR address type
this means an incoming connection. For LE Public and LE Random address types, a
connection will be established to devices using directed advertising. If
successful a Device Connected event will be sent.

With the Action 2, when the device is found, it will be connected and if
successful a Device Connected event will be sent. This action is only valid for
LE Public and LE Random address types.

When a device is blocked using Block Device command, then it is valid to add the
device here, but all actions will be ignored until the device is unblocked.

Devices added with Action 1 are allowed to connect even if the connectable
setting is off. This acts as list of known trusted devices.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Failed:
:Invalid Parameters:
:Invalid Index:

Remove Device (since 1.7)
`````````````````````````

:Command Code:		0x0034
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to remove a device from the action list previously added by
using the Add Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

When the Address parameter is 00:00:00:00:00:00, then all previously added
devices will be removed.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Load Connection Parameters (since 1.7)
``````````````````````````````````````

:Command Code:		0x0035
:Controller Index:	<controller id>
:Command Parameters:	Param_Count (2 Octets)
:...:			Address[] (6 Octets)
:...:			Address_Type[] (1 Octet)
:...:			Min_Connection_Interval[] (2 Octets)
:...:			Max_Connection_Interval[] (2 Octets)
:...:			Connection_Latency[] (2 Octets)
:...:			Supervision_Timeout[] (2 Octets)
:...:			...[]
:Return Parameters:

This command is used to load connection parameters from several devices into
kernel. Currently this is only supported on controllers with Low Energy support.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

The Min_Connection_Interval, Max_Connection_Interval, Connection_Latency and
Supervision_Timeout parameters should be configured as described in Core 4.1
spec, Vol 2, 7.8.12.

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:
:Not Supported:

Read Unconfigured Controller Index List (since 1.7)
```````````````````````````````````````````````````

:Command Code:		0x0036
:Controller Index:	<non-controller>
:Command Parameters:
:Return Parameters:	Num_Controllers (2 Octets)
:...:			Controller_Index[i] (2 Octets)

This command returns the list of currently unconfigured controllers.
Unconfigured controllers added after calling this command can be monitored using
the Unconfigured Index Added event.

An unconfigured controller can either move to a configured state by indicating
Unconfigured Index Removed event followed by an Index Added event; or it can be
removed from the system which would be indicated by the Unconfigured Index
Removed event.

Only controllers that require configuration will be listed with this command. A
controller that is fully configured will not be listed even if it supports
configuration changes.

This command generates a Command Complete event on success or a Command Status
event on failure.

Read Controller Configuration Information (since 1.7)
`````````````````````````````````````````````````````

:Command Code:		0x0037
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Manufacturer (2 Octets)
:...:			Supported_Options (4 Octets)
:...:			Missing_Options (4 Octets)

This command is used to retrieve the supported configuration options of a
controller and the missing configuration options.

The missing options are required to be configured before the controller is
considered fully configured and ready for standard operation. The command is
typically used right after getting the response to Read Unconfigured Controller
Index List command or Unconfigured Index Added event.

Supported_Options and Missing_Options is a bitmask with currently
the following available bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, External configuration
	1, Bluetooth public address configuration

It is valid to call this command on controllers that do not require any
configuration. It is possible that a fully configured controller offers
additional support for configuration.

For example a controller may contain a valid Bluetooth public device address,
but also allows to configure it from the host stack. In this case the general
support for configurations will be indicated by the Controller Configuration
settings. For controllers where no configuration options are available that
setting option will not be present.

When all configurations have been completed and as a result the Missing_Options
mask would become empty, then the now ready controller will be announced via
Index Added event.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set External Configuration (since 1.7)
``````````````````````````````````````

:Command Code:		0x0038
:Controller Index:	<controller id>
:Command Parameters:	Configuration (1 Octet)
:Return Parameters:	Missing_Options (4 Octets)

This command allows to change external configuration option to indicate that a
controller is now configured or unconfigured.

The value 0x00 sets unconfigured state and the value 0x01 sets configured state
of the controller.

It is not mandatory that this configuration option is provided by a controller.
If it is provided, the configuration has to happen externally using user channel
operation or via vendor specific methods.

Setting this option and when Missing_Options returns zero, this means that the
controller will switch to configured state and it can be expected that it will
be announced via Index Added event.

Wrongly configured controllers might still cause an error when trying to power
them via Set Powered command.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Set Public Address (since 1.7)
``````````````````````````````

:Command Code:		0x0039
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:Return Parameters:	Missing_Options (4 Octets)

This command allows configuration of public address. Since a vendor specific
procedure is required, this command might not be supported by all controllers.
Actually most likely only a handful embedded controllers will offer support for
this command.

When the support for Bluetooth public address configuration is indicated in the
supported options mask, then this command can be used to configure the public
address.

It is only possible to configure the public address when the controller is
powered off.

For an unconfigured controller and when Missing_Options returns an empty mask,
this means that a Index Added event for the now fully configured controller can
be expected.

For a fully configured controller, the current controller index will become
invalid and an Unconfigured Index Removed event will be sent. Once the address
has been successfully changed an Index Added event will be sent. There is no
guarantee that the controller index stays the same.

All previous configured parameters and settings are lost when this command
succeeds. The controller has to be treated as new one. Use this command for a
fully configured controller only when you really know what you are doing.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Start Service Discovery (since 1.8)
```````````````````````````````````

:Command Code:		0x003a
:Controller Index:	<controller id>
:Command Parameters:	Address_Type (1 Octet)
:...:			RSSI_Threshold (1 Octet)
:...:			UUID_Count (2 Octets)
:...:			UUID[i] (16 Octets)
:Return Parameters:	Address_Type (1 Octet)

This command is used to start the process of discovering remote devices with a
specific UUID. A Device Found event will be sent for each discovered device.

Possible values for the Address_Type parameter are a bit-wise or of the
following bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

By combining these e.g. the following values are possible:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x01, BR/EDR
	0x06, LE (public & random)
	0x07, BR/EDR/LE (interleaved discovery)

The service discovery uses active scanning for Low Energy scanning and will
search for UUID in both advertising data and scan response data.

Found devices that have a RSSI value smaller than RSSI_Threshold are not
reported via DeviceFound event. Setting a value of 127 will cause all devices to
be reported.

The list of UUIDs identifies a logical OR. Only one of the UUIDs have to match
to cause a DeviceFound event. Providing an empty list of UUIDs with Num_UUID set
to 0 which means that DeviceFound events are send out for all devices above the
RSSI_Threshold.

In case RSSI_Threshold is set to 127 and UUID_Count is 0, then this command
behaves exactly the same as Start Discovery.

When the discovery procedure starts the Discovery event will notify this similar
to Start Discovery.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Read Local Out Of Band Extended Data (since 1.9)
````````````````````````````````````````````````

:Command Code:		0x003b
:Controller Index:	<controller id>
:Command Parameters:	Address_Type (1 Octet)
:Return Parameters:	Address_Type (1 Octet)
:...:			EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This command is used to read the local Out of Band data information and provide
them encoded as extended inquiry response information or advertising data.

Possible values for the Address_Type parameter are a bit-wise or of the
following bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

By combining these e.g. the following values are possible:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x01, BR/EDR
	0x06, LE (public & random)
	0x07, Reserved (not in use)

For BR/EDR controller (Address_Type 1) the returned information will contain the
following information:

	Class of Device
	Simple Pairing Hash C-192 (optional)
	Simple Pairing Randomizer R-192 (optional)
	Simple Pairing Hash C-256 (optional)
	Simple Pairing Randomizer R-256 (optional)
	Service Class UUID (optional)
	Bluetooth Local Name (optional)

The Simple Pairing Hash C-256 and Simple Pairing Randomizer R-256 fields are
only included when secure connections has been enabled.

The Device Address (BD_ADDR) is not included in the EIR_Data and needs to be
taken from controller information.

For LE controller (Address_Type 6) the returned information will contain the
following information:

	LE Bluetooth Device Address
	LE Role
	LE Secure Connections Confirmation Value (optional)
	LE Secure Connections Random Value (optional)
	Appearance (optional)
	Local Name (optional)
	Flags

The LE Secure Connections Confirmation Value and LE Secure Connections Random
Value fields are only included when secure connections has been enabled.

The Security Manager TK Value from the Bluetooth specification can not be
provided by this command. The Out Of Band information here are for asymmetric
exchanges based on Diffie-Hellman key exchange. The Security Manager TK Value is
a symmetric random number that has to be acquired and agreed upon differently.

The returned information from BR/EDR controller and LE controller types are not
related to each other. Once they have been used over an Out Of Band link, a new
set of information shall be requested.

When Secure Connections Only mode has been enabled, then the fields for Simple
Pairing Hash C-192 and Simple Pairing Randomizer R-192 are not returned. Only
the fields for the strong secure connections pairing are included.

This command can only be used when the controller is powered.

Values returned by this command become invalid when the controller is powered
down. After each power-cycle it is required to call this command again to get
updated information.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Not Supported:
:Busy:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Read Extended Controller Index List (since 1.9)
```````````````````````````````````````````````

:Command Code:		0x003c
:Controller Index:	<non-controller>
:Command Parameters:
:Return Parameters:	Num_Controllers (2 Octets)
:...:			Controller_Index[i] (2 Octets)
:...:			Controller_Type[i] (1 Octet)
:...:			Controller_Bus[i] (1 Octet)

This command returns the list of currently known controllers. It includes
configured, unconfigured and alternate controllers.

Controllers added or removed after calling this command can be monitored using
the Extended Index Added and Extended Index Removed events.

The existing Index Added, Index Removed, Unconfigured Index Added and
Unconfigured Index Removed are no longer sent after this command has been used
at least once.

Instead of calling Read Controller Index List and Read Unconfigured Controller
Index List, this command combines all the information and can be used to
retrieve the controller list.

The Controller_Type parameter has these values:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Primary Controller (BR/EDR and/or LE)
	0x01, Unconfigured Controller (BR/EDR and/or LE)
	0x02, Alternate MAC/PHY Controller (AMP)

The 0x00 and 0x01 types indicate a primary BR/EDR and/or LE controller. The
difference is just if they need extra configuration or if they are fully
configured.

Controllers in configured state will be listed as 0x00 and controllers in
unconfigured state will be listed as 0x01. A controller that is fully configured
and supports configuration changes will be listed as 0x00.

Alternate MAC/PHY controllers will be listed as 0x02. They do not support the
difference between configured and unconfigured state.

The Controller_Bus parameter has these values:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Virtual
	0x01, USB
	0x02, PCMCIA
	0x03, UART
	0x04, RS232
	0x05, PCI
	0x06, SDIO
	0x07, SPI
	0x08, I2C
	0x09, SMD
	0x0A, VIRTIO
	0x0B, IPC

Controllers marked as RAW only operation are currently not listed by this
command.

This command generates a Command Complete event on success or a Command Status
event on failure.

Read Advertising Features (since 1.9)
`````````````````````````````````````

:Command Code:		0x003d
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Supported_Flags (4 Octets)
:...:			Max_Adv_Data_Len (1 Octet)
:...:			Max_Scan_Rsp_Len (1 Octet)
:...:			Max_Instances (1 Octet)
:...:			Num_Instances (1 Octet)
:...:			Instance[] (1 Octet)
:...:			...[]

This command is used to read the advertising features supported
by the controller and stack.

With the Supported_Flags field the possible values for the Flags
field in Add Advertising command provided:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Switch into Connectable mode
	1, Advertise as Discoverable
	2, Advertise as Limited Discoverable
	3, Add Flags field to Adv_Data
	4, Add TX Power field to Adv_Data
	5 (since 1.14), Add Appearance field to Scan_Rsp
	6 (since 1.14), Add Local Name in Scan_Rsp
	7 (since 1.15), Secondary Channel with LE 1M
	8 (since 1.15), Secondary Channel with LE 2M
	9 (since 1.15), Secondary Channel with LE Coded

The Flags bit 0 indicates support for connectable advertising and for switching
to connectable advertising independent of the connectable global setting. When
this flag is not supported, then the global connectable setting determines if
undirected connectable, undirected scannable or undirected non-connectable
advertising is used. It also determines the use of non-resolvable random address
versus identity address or resolvable private address.

The Flags bit 1 indicates support for advertising with discoverable mode
enabled. Users of this flag will decrease the Max_Adv_Data_Len by 3 octets. In
this case the advertising data flags are managed and added in front of the
provided advertising data.

The Flags bit 2 indicates support for advertising with limited discoverable mode
enabled. Users of this flag will decrease the Max_Adv_Data_Len by 3 octets. In
this case the advertising data flags are managed and added in front of the
provided advertising data.

The Flags bit 3 indicates support for automatically keeping the Flags field of
the advertising data updated. Users of this flag will decrease the
Max_Adv_Data_Len by 3 octets and need to keep that in mind. The Flags field will
be added in front of the advertising data provided by the user. Note that with
Flags bit 1 and Flags bit 2, this one will be implicitly used even if it is not
marked as supported.

The Flags bit 4 indicates support for automatically adding the TX Power value to
the advertising data. Users of this flag will decrease the Max_Adv_Data_Len by 3
octets. The TX Power field will be added at the end of the user provided
advertising data. If the controller does not support TX Power information, then
this bit will not be set.

The Flags bit 5 indicates support for automatically adding the Appearance value
to the scan response data. Users of this flag will decrease the Max_Scan_Rsp_len
by 4 octets. The Appearance field will be added in front of the scan response
data provided by the user. If the appearance value is not supported, then this
bit will not be set.

The Flags bit 6 indicates support for automatically adding the Local Name value
to the scan response data. This flag indicates an opportunistic approach for the
Local Name. If enough space in the scan response data is available, it will be
added. If the space is limited a short version or no name information. The Local
Name will be added at the end of the scan response data.

The Flags bit 7 indicates support for advertising in secondary channel in LE 1M
PHY.

The Flags bit 8 indicates support for advertising in secondary channel in LE 2M
PHY. Primary channel would be on 1M.

The Flags bit 9 indicates support for advertising in secondary channel in LE
CODED PHY.

The valid range for Instance identifiers is 1-254. The value 0 is reserved for
internal use and the value 255 is reserved for future extensions. However the
Max_Instances value for indicating the number of supported Instances can be also
0 if the controller does not support any advertising.

The Max_Adv_Data_Len and Max_Scan_Rsp_Len provides extra information about the
maximum length of the data fields. For now this will always return the value 31.
Different flags however might decrease the actual available length in these data
fields.

With Num_Instances and Instance array the currently occupied Instance
identifiers can be retrieved.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Add Advertising (since 1.9)
```````````````````````````

:Command Code:		0x003e
:Controller Index:	<controller id>
:Command Parameters:	Instance (1 Octet)
:...:			Flags (4 Octets)
:...:			Duration (2 Octets)
:...:			Timeout (2 Octets)
:...:			Adv_Data_Len (1 Octet)
:...:			Scan_Rsp_Len (1 Octet)
:...:			Adv_Data (0-255 Octets)
:...:			Scan_Rsp (0-255 Octets)
:Return Parameters:	Instance (1 Octet)

This command is used to configure an advertising instance that can be used to
switch a Bluetooth Low Energy controller into advertising mode.

Added advertising information with this command will not be visible immediately
if advertising is enabled via the Set Advertising command. The usage of the Set
Advertising command takes precedence over this command. Instance information is
stored and will be advertised once advertising via Set Advertising has been
disabled.

The Instance identifier is a value between 1 and the number of supported
instances. The value 0 is reserved.

With the Flags value the type of advertising is controlled and the following
flags are defined:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Switch into Connectable mode
	1, Advertise as Discoverable
	2, Advertise as Limited Discoverable
	3, Add Flags field to Adv_Data
	4, Add TX Power field to Adv_Data
	5 (since 1.14), Add Appearance field to Scan_Rsp
	6 (since 1.14), Add Local Name in Scan_Rsp
	7 (since 1.15), Secondary Channel with LE 1M
	8 (since 1.15), Secondary Channel with LE 2M
	9 (since 1.15), Secondary Channel with LE Coded

When the connectable flag is set, then the controller will use undirected
connectable advertising. The value of the connectable setting can be overwritten
this way. This is useful to switch a controller into connectable mode only for
LE operation. This is similar to the mode 0x02 from the Set Advertising command.

When the connectable flag is not set, then the controller will use advertising
based on the connectable setting. When using non-connectable or scannable
advertising, the controller will be programmed with a non-resolvable random
address. When the system is connectable, then the identity address or resolvable
private address will be used.

Using the connectable flag is useful for peripheral mode support where BR/EDR
(and/or LE) is controlled by Add Device. This allows making the peripheral
connectable without having to interfere with the global connectable setting.

If Scan_Rsp_Len is zero and connectable flag is not set and the global
connectable setting is off, then non-connectable advertising is used. If
Scan_Rsp_Len is larger than zero and connectable flag is not set and the global
advertising is off, then scannable advertising is used. This small difference is
supported to provide less air traffic for devices implementing broadcaster role.

Secondary channel flags can be used to advertise in secondary channel with the
corresponding PHYs. These flag bits are mutually exclusive and setting multiple
will result in Invalid Parameter error. Choosing either LE 1M or LE 2M will
result in using extended advertising on the primary channel with LE 1M and the
respectively LE 1M or LE 2M on the secondary channel. Choosing LE Coded will
result in using extended advertising on the primary and secondary channels with
LE Coded. Choosing none of these flags will result in legacy advertising.

The Duration parameter configures the length of an Instance. The value is in
seconds.

A value of 0 indicates a default value is chosen for the Duration. The default
is 2 seconds.

If only one advertising Instance has been added, then the Duration value will be
ignored. It only applies for the case where multiple Instances are configured.
In that case every Instance will be available for the Duration time and after
that it switches to the next one. This is a simple round-robin based approach.

The Timeout parameter configures the life-time of an Instance. In case the value
0 is used it indicates no expiration time. If a timeout value is provided, then
the advertising Instance will be automatically removed when the timeout passes.
The value for the timeout is in seconds. Powering down a controller will
invalidate all advertising Instances and it is not possible to add a new
Instance with a timeout when the controller is powered down.

When a Timeout is provided, then the Duration subtracts from the actual Timeout
value of that Instance. For example an Instance with Timeout of 5 and Duration
of 2 will be scheduled exactly 3 times, twice with 2 seconds and once with one
second. Other Instances have no influence on the Timeout.

Re-adding an already existing instance (i.e. issuing the Add Advertising command
with an Instance identifier of an existing instance) will update that instance's
configuration.

An instance being added or changed while another instance is being advertised
will not be visible immediately but only when the new/changed instance is being
scheduled by the round robin advertising algorithm.

Changes to an instance that is currently being advertised will cancel that
instance and switch to the next instance. The changes will be visible the next
time the instance is scheduled for advertising. In case a single instance is
active, this means that changes will be visible right away.

A pre-requisite is that LE is already enabled, otherwise this command will
return a "rejected" response.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Failed:
:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Remove Advertising (since 1.9)
``````````````````````````````

:Command Code:		0x003f
:Controller Index:	<controller id>
:Command Parameters:	Instance (1 Octet)
:Return Parameters:	Instance (1 Octet)

This command is used to remove an advertising instance that can be used to
switch a Bluetooth Low Energy controller into advertising mode.

When the Instance parameter is zero, then all previously added advertising
Instances will be removed.

Removing advertising information with this command will not be visible as long
as advertising is enabled via the Set Advertising command. The usage of the Set
Advertising command takes precedence over this command. Changes to Instance
information are stored and will be advertised once advertising via Set
Advertising has been disabled.

Removing an instance while it is being advertised will immediately cancel the
instance, even when it has been advertised less then its configured Timeout or
Duration.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Get Advertising Size Information (since 1.11)
`````````````````````````````````````````````

:Command Code:		0x0040
:Controller Index:	<controller id>
:Command Parameters:	Instance (1 Octet)
:...:			Flags (4 Octets)
:Return Parameters:	Instance (1 Octet)
:...:			Flags (4 Octets)
:...:			Max_Adv_Data_Len (1 Octet)
:...:			Max_Scan_Rsp_Len (1 Octet)

The Read Advertising Features command returns the overall maximum size of
advertising data and scan response data fields. That size is valid when no Flags
are used. However when certain Flags are used, then the size might decrease.
This command can be used to request detailed information about the maximum
available size.

The following Flags values are defined:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Switch into Connectable mode
	1, Advertise as Discoverable
	2, Advertise as Limited Discoverable
	3, Add Flags field to Adv_Data
	4, Add TX Power field to Adv_Data
	5, Add Appearance field to Scan_Rsp
	6, Add Local Name in Scan_Rsp

To get accurate information about the available size, the same Flags values
should be used with the Add Advertising command.

The Max_Adv_Data_Len and Max_Scan_Rsp_Len fields provide information about the
maximum length of the data fields for the given Flags values. When the Flags
field is zero, then these fields would contain the same values as Read
Advertising Features.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Start Limited Discovery (since 1.11)
````````````````````````````````````

:Command Code:		0x0041
:Controller Index:	<controller id>
:Command Parameters:	Address_Type (1 Octet)
:Return Parameters:	Address_Type (1 Octet)

This command is used to start the process of discovering remote devices using
the limited discovery procedure. A Device Found event will be sent for each
discovered device.

Possible values for the Address_Type parameter are a bit-wise or
of the following bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

By combining these e.g. the following values are possible:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x01, BR/EDR
	0x06, LE (public & random)
	0x07, BR/EDR/LE (interleaved discovery)

The limited discovery uses active scanning for Low Energy scanning and will
search for devices with the limited discoverability flag configured. On BR/EDR
it uses LIAC and filters on the limited discoverability flag of the class of
device.

When the discovery procedure starts the Discovery event will notify this similar
to Start Discovery.

This command can only be used when the controller is powered.

This command generates a Command Complete event on success or failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Not Powered:
:Invalid Index:

Read Extended Controller Information (since 1.14)
`````````````````````````````````````````````````

:Command Code:		0x0042
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Address (6 Octets)
:...:			Bluetooth_Version (1 Octet)
:...:			Manufacturer (2 Octets)
:...:			Supported_Settings (4 Octets)
:...:			Current_Settings (4 Octets)
:...:			EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This command is used to retrieve the current state and basic information of a
controller. It is typically used right after getting the response to the Read
Controller Index List command or an Index Added event (or its extended
counterparts).

The Address parameter describes the controllers public address and it can be
expected that it is set. However in case of single mode Low Energy only
controllers it can be 00:00:00:00:00:00. To power on the controller in this
case, it is required to configure a static address using Set Static Address
command first.

If the public address is set, then it will be used as identity address for the
controller. If no public address is available, then the configured static
address will be used as identity address.

In the case of a dual-mode controller with public address that is configured as
Low Energy only device (BR/EDR switched off), the static address is used when
set and public address otherwise.

Current_Settings and Supported_Settings is a bitmask with
currently the following available bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Powered
	1, Connectable
	2, Fast Connectable
	3, Discoverable
	4, Bondable
	5, Link Level Security (Sec. mode 3)
	6, Secure Simple Pairing
	7, Basic Rate/Enhanced Data Rate
	8, High Speed
	9, Low Energy
	10, Advertising
	11, Secure Connections
	12, Debug Keys
	13, Privacy
	14, Controller Configuration
	15, Static Address
	16, PHY Configuration
	17, Wideband Speech
	18, Connected Isochronous Stream - Central
	19, Connected Isochronous Stream - Peripheral

The EIR_Data field contains information about class of device, local name and
other values. Not all of them might be present. For example a Low Energy only
device does not contain class of device information.

When any of the values in the EIR_Data field changes, the event Extended
Controller Information Changed will be used to inform clients about the updated
information.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Appearance (since 1.14)
```````````````````````````

:Command Code:		0x0043
:Controller Index:	<controller id>
:Command Parameters:	Appearance (2 Octets)
:Return Parameters:

This command is used to set the appearance value of a controller.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

The value of appearance will be remembered when switching the controller off and
back on again. So the appearance only have to be set once when a new controller
is found and will stay until removed.

This command generates a Command Complete event on success or a Command Status
event on failure.

This command is only available for LE capable controllers.

It will return Not Supported otherwise.

Possible errors:

:Not Supported:
:Invalid Parameters:
:Invalid Index:

Get PHY Configuration (since 1.15)
``````````````````````````````````

:Command Code:		0x0044
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Supported_PHYs (4 Octet)
:...:			Configurable_PHYs (4 Octets)
:...:			Selected_PHYs (4 Octet)

The PHYs parameters are a bitmask with currently the following available bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR 1M 1-Slot
	1, BR 1M 3-Slot
	2, BR 1M 5-Slot
	3, EDR 2M 1-Slot
	4, EDR 2M 3-Slot
	5, EDR 2M 5-Slot
	6, ERDR 3M 1-Slot
	7, EDR 3M 3-Slot
	8, EDR 3M 5-Slot
	9, LE 1M TX
	10, LE 1M RX
	11, LE 2M TX
	12, LE 2M RX
	13, LE Coded TX
	14, LE Coded RX

If BR/EDR is supported, then BR 1M 1-Slot is supported by default and can also
not be deselected. If LE is supported, then LE 1M TX and LE 1M RX are supported
by default.

Disabling BR/EDR completely or respectively LE has no impact on the PHY
configuration. It is remembered over power cycles.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set PHY Configuration (since 1.15)
``````````````````````````````````

:Command Code:		0x0045
:Controller Index:	<controller id>
:Command Parameters: 	Selected_PHYs (4 Octet)
:Return Parameters:

This command is used to set the default PHY to the controller.

This will be stored and used for all the subsequent scanning and connection
initiation.

The list of supported PHYs can be retrieved via the Get PHY Configuration
command. Selecting unsupported or deselecting default PHYs will result in an
Invalid Parameter error.

This can be called at any point to change the Selected PHYs.

Refer Get PHY Configuration command for PHYs parameter.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Load Blocked Keys (since 1.15)
``````````````````````````````

:Command Code:		0x0046
:Controller Index:	<controller id>
:Command Parameters:	Key_Count (2 Octets)
:...:			Key_Type[] (1 Octet)
:...:			Value[] (16 Octets)
:...:			...[]
:Return Parameters:

This command is used to feed the kernel a list of keys that are known to be
vulnerable.

If the pairing procedure produces any of these keys, they will be silently
dropped and any attempt to enable encryption rejected.

Currently defined Key_Type values are:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Link Key (BR/EDR)
	0x01, Long Term Key (LE)
	0x02, Identity Resolving Key (LE)

This command can be used when the controller is not powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Wideband Speech (since 1.16)
````````````````````````````````

:Command Code:		0x0047
:Controller Index:	<controller id>
:Command Parameters:	Wideband_Speech (1 Octet)
:Return Parameters:	Current_Settings (4 Octets)

This command is used to enable/disable Wideband Speech support for a controller.
The allowed values for the Wideband_Speech command parameter are 0x00 and 0x01.
All other values will return Invalid Parameters.

The value 0x00 disables Wideband Speech, the value 0x01 enables Wideband Speech.

This command is only available for BR/EDR capable controllers and require
controller specific support.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

In case the controller does not support Wideband Speech the command will fail
regardless with Not Supported error.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Busy:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Read Controller Capabilities (since 1.17)
`````````````````````````````````````````

:Command Code:		0x0048
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Capabilities_Data_Length (2 Octets)
:...:			Capabilities_Data[] (0-65535 Octets)

This command is used to retrieve the supported capabilities by the controller
or the host stack.

The Capabilities_Data_Length and Capabilities_Data parameters provide a list of
security settings, features and information. It uses the same format as
EIR_Data, but with the namespace defined here.

.. csv-table::
	:header: "Data Type", "Name"
	:widths: auto

	0x01, Flags
	0x02, Max Encryption Key Size (BR/EDR)
	0x03, Max Encryption Key Size (LE)
	0x04, Supported Tx Power (LE)

Flags (data type 0x01)

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Remote public key validation (BR/EDR)
	1, Remote public key validation (LE)
	2, Encryption key size enforcement (BR/EDR)
	3, Encryption key size enforcement (LE)

Max Encryption Key Size (data types 0x02 and 0x03)

	When the field is present, then it provides 1 Octet value indicating the
	max encryption key size. If the field is not present, then it is unknown
	what the max encryption key size of the controller or host is in use.

Supported LE Tx Power (data type 0x04)

	When present, this 2-octet field provides the min and max LE Tx power
	supported by the controller, respectively, as reported by the LE Read
	Transmit Power HCI command. If this field is not available, it indicates
	that the LE Read Transmit Power HCI command was not available.

This command generates a Command Complete event on success or
a Command Status event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Read Experimental Features Information (since 1.17)
```````````````````````````````````````````````````

:Command Code:		0x0049
:Controller Index:	<controller id> or <non-controller>
:Command Parameters:
:Return Parameters:	Feature_Count (2 Octets)
:...:			UUID[] (16 Octets)
:...:			Flags[] (4 Octets)
:...:			...[]

This command is used to retrieve the supported experimental features by the host
stack.

The UUID values are not defined here. They can change over time and are on
purpose not stable. Features that mature will be removed at some point. The
mapping of feature UUID to the actual functionality of a given feature is out of
scope here.

The following bits are defined for the Flags parameter:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Feature active
	1, Causes change in supported settings

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Experimental Feature (since 1.17)
`````````````````````````````````````

:Command Code:		0x004a
:Controller Index:	<controller id> or <non-controller>
:Command Parameters:	UUID (16 Octets)
:...:			Action (1 Octet)
:Return Parameters:	UUID (16 Octets)
:...:			Flags (4 Octets)

This command is used to change the setting of an experimental feature of the
host stack.

The UUID value must be a supported value returned from the Read Experimental
Features Information command.

The Action parameter is UUID specific, but in most cases it will be just a
simple on/off toggle with these values:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0x00, Disable feature
	0x01, Enable feature

It depends on the feature if the command can be used when the controller is
powered up. See Flags parameter of Read Experimental Features Information
command for details if the controller has to be powered down first.

The following bits are defined for the Flags return parameter:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Feature active
	1, Supported settings changed

When a feature causes the change of supported settings, then it is a good idea
to re-read the controller information.

When the UUID parameter is an empty UUID (16 x 0x00), then all experimental
features will be deactivated.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Not Powered:
:Invalid Index:

Read Default System Configuration (since 1.18)
``````````````````````````````````````````````

:Command Code:		0x004b
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value (0-255[] Octets)
:...:			...[]

This command is used to read a list of default controller parameters.

Currently defined Parameter_Type values are:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x0000, BR/EDR Page Scan Type
	0x0001, BR/EDR Page Scan Interval
	0x0002, BR/EDR Page Scan Window
	0x0003, BR/EDR Inquiry Scan Type
	0x0004, BR/EDR Inquiry Scan Interval
	0x0005, BR/EDR Inquiry Scan Window
	0x0006, BR/EDR Link Supervision Timeout
	0x0007, BR/EDR Page Timeout
	0x0008, BR/EDR Min Sniff Interval
	0x0009, BR/EDR Max Sniff Interval
	0x000a, LE Advertisement Min Interval
	0x000b, LE Advertisement Max Interval
	0x000c, LE Multi Advertisement Rotation Interval
	0x000d, LE Scanning Interval for auto connect
	0x000e, LE Scanning Window for auto connect
	0x000f, LE Scanning Interval for wake scenarios
	0x0010, LE Scanning Window for wake scenarios
	0x0011, LE Scanning Interval for discovery
	0x0012, LE Scanning Window for discovery
	0x0013, LE Scanning Interval for adv monitoring
	0x0014, LE Scanning Window for adv monitoring
	0x0015, LE Scanning Interval for connect
	0x0016, LE Scanning Window for connect
	0x0017, LE Min Connection Interval
	0x0018, LE Max Connection Interval
	0x0019, LE Connection Latency
	0x001a, LE Connection Supervision Timeout
	0x001b, LE Autoconnect Timeout

This command can be used at any time and will return a list of supported default
parameters as well as their current value.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Default System Configuration (since 1.18)
`````````````````````````````````````````````

:Command Code:		0x004c
:Controller Index:	<controller id>
:Command Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value[] (0-255 Octets)
:...:			...[]
:Return Parameters:

This command is used to set a list of default controller parameters.

See Read Default System Configuration command for list of supported
Parameter_Type values.

This command can be used when the controller is not powered and all supported
parameters will be programmed once powered.

When providing unsupported values or invalid values, no parameter value will be
changed and all values discarded.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Read Default Runtime Configuration (since 1.18)
```````````````````````````````````````````````

:Command Code:		0x004d
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value[] (0-255 Octets)
:...:			...[]

This command is used to read a list of default runtime parameters.

Currently no Parameter_Type values are defined and an empty list will be
returned.

This command can be used at any time and will return a list of supported default
parameters as well as their current value.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Default Runtime Configuration (since 1.18)
``````````````````````````````````````````````

:Command Code:		0x004e
:Controller Index:	<controller id>
:Command Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value[] (0-255 Octets)
:...:			...[]
:Return Parameters:

This command is used to set a list of default runtime parameters.

See Read Default Runtime Configuration command for list of supported
Parameter_Type values.

This command can be used at any time and will change the runtime default.
Changes however will not apply to existing connections or currently active
operations.

When providing unsupported values or invalid values, no parameter value will be
changed and all values discarded.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Rejected:
:Not Supported:
:Invalid Parameters:
:Invalid Index:

Get Device Flags (since 1.18)
`````````````````````````````

:Command Code:		0x004f
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Supported_Flags (4 Octets)
:...:			Current_Flags (4 Octets)

This command is used to retrieve additional flags and settings for devices that
are added via Add Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The Flags parameters are a bitmask with currently the following
available bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Remote Wakeup enabled
	1, Device Privacy Mode enabled
	2, Address Resolution enabled

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Set Device Flags (since 1.18)
`````````````````````````````

:Command Code:		0x0050
:Controller Index:	<controller id>
:Command Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Current_Flags (4 Octets)
:Return Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This command is used to configure additional flags and settings for devices that
are added via Add Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The list of supported Flags can be retrieved via the Get Device Flags or Device
Flags Changed command. Selecting unsupported flags will result in an Invalid
Parameter error;

Refer to the Get Device Flags command for a detailed description of the Flags
parameters.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Invalid Parameters:
:Invalid Index:

Read Advertisement Monitor Features (since 1.18)
````````````````````````````````````````````````

:Command Code:		0x0051
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Supported_Features (4 Octets)
:...:			Enabled_Features (4 Octets)
:...:			Max_Num_Handles (2 Octets)
:...:			Max_Num_Patterns (1 Octet)
:...:			Num_Handles (2 Octets)
:...:			Handle[] (2 Octets)
:...:			...[]

This command is used to read the advertisement monitor features supported by the
controller and stack. Supported_Features lists all related features supported by
the controller while Enabled_Features lists the ones currently used by the
kernel.

Supported_Features and Enabled_Features are bitmasks with currently the
following available bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Advertisement content monitoring based on patterns with logic OR.

Max_Num_Handles indicates the maximum number of supported advertisement
monitors. Note that the actual number of supported ones might be less depending
on the limitation of the controller.

Max_Num_Pattern indicates the maximum number of supported patterns in an
advertisement patterns monitor. Note that the actual number of supported ones
might be less depending on the limitation of the controller.

Num_Handles indicates the number of added advertisement monitors, and it is
followed by a list of handles.

This command can be used when the controller is not powered.

Add Advertisement Patterns Monitor (since 1.18)
```````````````````````````````````````````````

:Command Code:		0x0052
:Controller Index:	<controller id>
:Command Parameters:	Pattern_Count (1 Octet)
:...:			AD_Type[] (1 Octet)
:...:			Offset[] (1 Octet)
:...:			Length[] (1 Octet)
:...:			Value[] (31 Octets)
:...:			...[]
:Return Parameters:	Monitor_Handle (2 Octets)

This command is used to add an advertisement monitor whose filtering conditions
are patterns. The kernel will trigger scanning if there is at least one monitor
added. If the controller supports advertisement filtering, the kernel would
offload the content filtering to the controller in order to reduce power
consumption; otherwise the kernel ignores the content of the monitor. Note that
if the there are more than one patterns, OR logic would applied among patterns
during filtering. In other words, any advertisement matching at least one
pattern in a given monitor would be considered as a match.

A pattern contains the following fields:

:AD_Data_Type:	Advertising Data Type. The possible values are defined in Core
:...:		Specification Supplement.
:Offset:	The start index where pattern matching shall be performed with
:...:		in the AD data.
:Length:	The length of the pattern value in bytes.
:Value:		The value of the pattern in bytes.

Here is an example of a pattern.

.. code-block::

	{
		0x16, // Service Data - 16-bit UUID
		0x02, // Skip the UUID part.
		0x04, // Length of the value
		{0x11, 0x22, 0x33, 0x44},
	}

This command can be used when the controller is not powered and all settings
will be programmed once powered.

Possible errors:

:Failed:
:Busy:
:No Resources:
:Invalid Parameters:

Remove Advertisement Monitor (since 1.18)
`````````````````````````````````````````

:Command Code:		0x0053
:Controller Index:	<controller id>
:Command Parameters:	Monitor_Handle (2 Octets)
:Return Parameters:	Monitor_Handle (2 Octets)

This command is used to remove advertisement monitor(s). The kernel would remove
the monitor(s) with Monitor_Handle and update the LE scanning.

When the Monitor_Handle is set to zero, then all previously added handles will
be removed.

Removing a monitor while it is being added will be ignored.

This command can be used when the controller is not powered and all settings
will be programmed once powered.

Possible errors:

:Failed:
:Busy:

Add Extended Advertising Parameters (since 1.19)
````````````````````````````````````````````````

:Command Code:		0x0054
:Controller Index:	<controller id>
:Command Parameters:	Instance (1 Octet)
:...:			Flags (4 Octets)
:...:			Duration (2 Octets)
:...:			Timeout (2 Octets)
:...:			MinInterval (4 Octets)
:...:			MaxInterval (4 Octets)
:...:			TxPower (1 Octet)
:Return Parameters:	Instance (1 Octet)
:...:			TxPower (1 Octet)
:...:			MaxAdvDataLen (1 Octet)
:...:			MaxScanRspLen (1 Octet)

This command is used to configure the parameters for Bluetooth Low Energy
advertising instance. This command is expected to be followed by an Add Extended
Advertising Data command to complete and enable the advertising instance.

Added advertising information with this command will not be visible immediately
if advertising is enabled via the Set Advertising command. The usage of the Set
Advertising command takes precedence over this command. Instance information is
stored and will be advertised once advertising via Set Advertising has been
disabled.

The Instance identifier is a value between 1 and the number of supported
instances. The value 0 is reserved.

With the Flags value the type of advertising is controlled and the following
flags are defined:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Switch into Connectable mode
	1, Advertise as Discoverable
	2, Advertise as Limited Discoverable
	3, Add Flags field to Adv_Data
	4, Add TX Power field to Adv_Data
	5, Add Appearance field to Scan_Rsp
	6, Add Local Name in Scan_Rsp
	7, Secondary Channel with LE 1M
	8, Secondary Channel with LE 2M
	9, Secondary Channel with LE Coded
	10, Indicate tx power can be specified
	11, Indicate HW supports the advertising offload
	12, The Duration parameter should be used
	13, The Timeout parameter should be used
	14, The Interval parameters should be used
	15, The Tx Power parameter should be used
	16, The advertisement will contain a scan response

When the connectable flag is set, then the controller will use undirected
connectable advertising. The value of the connectable setting can be overwritten
this way. This is useful to switch a controller into connectable mode only for
LE operation. This is similar to the mode 0x02 from the Set Advertising command.

When the connectable flag is not set, then the controller will use advertising
based on the connectable setting. When using non-connectable or scannable
advertising, the controller will be programmed with a non-resolvable random
address. When the system is connectable, then the identity address or resolvable
private address will be used.

Using the connectable flag is useful for peripheral mode support where BR/EDR
(and/or LE) is controlled by Add Device. This allows making the peripheral
connectable without having to interfere with the global connectable setting.

Secondary channel flags can be used to advertise in secondary channel with the
corresponding PHYs. These flag bits are mutually exclusive and setting multiple
will result in Invalid Parameter error. Choosing either LE 1M or LE 2M will
result in using extended advertising on the primary channel with LE 1M and the
respectively LE 1M or LE 2M on the secondary channel. Choosing LE Coded will
result in using extended advertising on the primary and secondary channels with
LE Coded. Choosing none of these flags will result in legacy advertising.

To allow future parameters to be optionally extended in this structure, the
flags member has been used to specify which of the structure fields were
purposefully set by the caller. Unspecified parameters will be given sensible
defaults by the kernel before the advertisement is registered.

The Duration parameter configures the length of an Instance. The value is in
seconds. The default is 2 seconds.

If only one advertising Instance has been added, then the Duration value will be
ignored. It only applies for the case where multiple Instances are configured.
In that case every Instance will be available for the Duration time and after
that it switches to the next one. This is a simple round-robin based approach.

The Timeout parameter configures the life-time of an Instance. In case the value
0 is used it indicates no expiration time. If a timeout value is provided, then
the advertising Instance will be automatically removed when the timeout passes.
The value for the timeout is in seconds. Powering down a controller will
invalidate all advertising Instances and it is not possible to add a new
Instance with a timeout when the controller is powered down.

When a Timeout is provided, then the Duration subtracts from the actual Timeout
value of that Instance. For example an Instance with Timeout of 5 and Duration
of 2 will be scheduled exactly 3 times, twice with 2 seconds and once with one
second. Other Instances have no influence on the Timeout.

MinInterval and MaxInterval define the minimum and maximum advertising
intervals, with units as number of .625ms advertising slots. The Max interval is
expected to be greater than or equal to the Min interval, and both must have
values in the range [0x000020, 0xFFFFFF]. If either condition is not met, the
registration will fail.

The provided Tx Power parameter will only be used if the controller supports it,
which can be determined by the presence of the CanSetTxPower member of the Read
Advertising Features command.

The acceptable range for requested Tx Power is defined in the spec (Version 5.2
| Vol 4, Part E, page 2585) to be [-127, +20] dBm, and the controller will
select a power value up to the requested one. The transmission power selected by
the controller is not guaranteed to match the requested one, so the reply will
contain the power chosen by the controller. If the requested Tx Power is outside
the valid range, the registration will fail.

When flag bit 16 is enabled, it indicates that the subsequent request to set
advertising data will contain a scan response, and that the parameters should
set a PDU type that is scannable.

Re-adding an already existing instance (i.e. issuing the Add Extended
Advertising Parameters command with an Instance identifier of an existing
instance) will update that instance's configuration. In this case where no new
instance is added, no Advertising Added event will be generated. However, if the
update of the instance fails, the instance will be removed, and an Advertising
Removed event will be generated.

An instance being added or changed while another instance is being advertised
will not be visible immediately but only when the new/changed instance is being
scheduled by the round robin advertising algorithm.

Changes to an instance that is currently being advertised will cancel that
instance and switch to the next instance. The changes will be visible the next
time the instance is scheduled for advertising. In case a single instance is
active, this means that changes will be visible right away.

The MaxAdvDataLen return parameter indicates how large the data payload can be
in the subsequent Add Extended Advertising Data Command, as it accounts for the
data required for the selected flags. Similarly, the MaxScanRspLen return
parameter indicates how large the scan response can be.

LE must already be enabled, and the controller must be powered, otherwise a
"rejected" status will be returned.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Failed:
:Rejected:
:Not Supported:
:Invalid Parameters:
:Busy:


Add Extended Advertising Data (since 1.19)
``````````````````````````````````````````

:Command Code:		0x0055
:Controller Index:	<controller id>
:Command Parameters:	Instance (1 Octet)
:...:			Advertising Data Length (1 Octet)
:...:			Scan Response Length (1 Octet)
:...:			Advertising Data (0-255 Octets)
:...:			Scan Response (0-255 Octets)
:Return Parameters:	Instance (1 Octet)

The Add Extended Advertising Data command is used to update the advertising data
of an existing advertising instance known to the kernel. It is expected to be
called after an Add Extended Advertising Parameters command, as part of the
advertisement registration process.

If extended advertising is available, this call will initiate HCI commands to
set the instance's advertising data, set scan response data, and then enable the
instance. If extended advertising is unavailable, the advertising instance
structure maintained in kernel will have its advertising data and scan response
updated, and the instance will either be scheduled immediately or left in the
queue for later advertisement as part of round-robin advertisement rotation in
software.

If Scan_Rsp_Len is zero and the flags defined in Add Extended Advertising
Parameters command do not have connectable flag set and the global connectable
setting is off, then non-connectable advertising is used. If Scan_Rsp_Len is
larger than zero and connectable flag is not set and the global advertising is
off, then scannable advertising is used. This small difference is supported to
provide less air traffic for devices implementing broadcaster role.

If the Instance provided does not match a known instance, or if the provided
advertising data or scan response are in an unrecognized format, an
"Invalid Parameters" status will be returned.

If a "Set LE" or Advertising command is still in progress, a "Busy" status will
be returned.

If the controller is not powered, a "rejected" status will be returned.

This command generates a Command Complete event on success or a Command Status
event on failure.

Possible errors:

:Failed:
:Rejected:
:Invalid Parameters:
:Busy:

Add Advertisement Patterns Monitor With RSSI Threshold (since 1.20)
```````````````````````````````````````````````````````````````````

:Command Code:		0x0056
:Controller Index:	<controller id>
:Command Parameters:	High_Threshold (1 Octet)
:...:			High_Threshold_Timer (2 Octets)
:...:			Low_Threshold (1 Octet)
:...:			Low_Threshold_Timer (2 Octets)
:...:			Sampling_Period (1 Octet)
:...:			Pattern_Count (1 Octet)
:...:			AD_Type[] (1 Octet)
:...:			Offset[] (1 Octet)
:...:			Length[] (1 Octet)
:...:			Value[] (31 Octets)
:...:			...[]
:Return Parameters:	Monitor_Handle (2 Octets)

This command is essentially the same as Add Advertisement Patterns Monitor
Command (0x0052), but with an additional RSSI parameters.
As such, if the kernel supports advertisement filtering, then the advertisement
data will be filtered in accordance with the set RSSI parameters. Otherwise, it
would behave exactly the same as the Add Advertisement Patterns Monitor Command.

Devices would be considered "in-range" if the RSSI of the received adv packets
are greater than High_Threshold dBm for High_Threshold_Timer seconds. Similarly,
devices would be considered lost if no received adv have RSSI greater than
Low_Threshold dBm for Low_Threshold_Timer seconds. Only adv packets of
"in-range" device would be propagated.

The meaning of Sampling_Period is as follows:

:0x00:		All adv packets from "in-range" devices would be propagated.
:0xFF:		Only the first adv data of "in-range" devices would be
:...:		propagated. If the device becomes lost, then the first data when
:...:		it is found again will also be propagated.
:other:		Advertisement data would be grouped into 100ms * N time period.
		Data in the same group will only be reported once, with the RSSI
		value being averaged out.

Possible errors:

:Failed:
:Busy:
:No Resources:
:Invalid Parameters:


Set Mesh Receiver (since 1.21)
``````````````````````````````

:Command Code:		0x0057
:Controller Index:	<controller id>
:Command Parameters:	Enable (1 Octets)
:...:			Window (2 Octets)
:...:			Period (2 Octets)
:...:			Num AD Types (1 Octets)
:...:			AD Types[]

This command Enables or Disables Mesh Receiving. When enabled passive scanning
remains enabled for this controller.

The Window/Period values are used to set the Scan Parameters when no other
scanning is being done.

Num AD Types and AD Types parameter, filter Advertising and Scan responses by AD
type. Responses that do not contain at least one of the requested AD types will
be ignored. Otherwise they will be delivered with the Mesh Device Found event.

Possible errors:

:Failed:
:No Resources:
:Invalid Parameters:

Read Mesh Features (since 1.21)
```````````````````````````````

:Command Code:		0x0058
:Controller Index:	<controller id>
:Command Parameters:
:Return Parameters:	Index (2 Octets)
:...:			Max Handles (1 Octets)
:...:			Used Handles (1 Octets)
:...:			Handle[]

This command is used to both verify that Outbound Mesh packet support is
enabled, and to indicate the number of packets that can and are simultaneously
queued.

Index identifies the HCI Controller that this information is valid for.

Max Handles indicates the maximum number of packets that may be queued.

Used Handles indicates the number of packets awaiting transmission.

Handle is an array of the currently outstanding packets.

Possible errors:

:Failed:
:No Resources:
:Invalid Parameters:

Transmit Mesh Packet (since 1.21)
`````````````````````````````````

:Command Code:		0x0059
:Controller Index:	<controller id>
:Command Parameters:	Addr (6 octets)
:...:			Addr Type (1 Octets)
:...:			Instant (8 Octets)
:...:			Delay (2 Octets)
:...:			Count (1 Octets)
:...:			Data Length (1 Octets)
:...:			Data[] (variable)
:Return Parameters:	Handle (1 Octets)

This command sends a Mesh Packet as a NONCONN LE Advertisement.

The Addr + Addr Type parameters specify the address to use in the outbound
advertising packet. If BD_ADDR_ANY and LE_RANDOM is set, the kernel will create
a single use non-resolvable address.

The Instant parameter is used in combination with the Delay parameter, to finely
time the sending of the Advertising packet. It should be set to the Instant
value tag of a received incoming Mesh Device Found Event. It is only useful in
POLL-RESPONSE situations where a response must be sent within a negotiated time
window. The value of the Instant parameter should not be interpreted by the
host, and only has meaning to the controller.

The Delay parameter, if 0x0000, will cause the packet to be sent at the earliest
opportunity. If non-Zero, and the controller supports delayed delivery, the
Instant and Delay parameters will be used to delay the outbound packet. While
the Instant is not defined, the Delay is specified in milliseconds.

The Count parameter must be sent to a non-Zero value indicating the number of
times this packet will be sent before transmission completes. If the Delay
parameter is non-Zero, then Count must be 1 only.

The Data parameter is an octet array of the AD Type and Mesh Packet.

This command will return immediately, and if it succeeds, will generate a Mesh
Packet Transmission Complete event when after the packet has been sent.

Possible errors:

:Failed:
:Busy:
:No Resources:
:Invalid Parameters:

Cancel Transmit Mesh Packet (since 1.21)
````````````````````````````````````````

:Command Code:		0x005A
:Controller Index:	<controller id>
:Command Parameters:	Handle (1 Octets)

This command may be used to cancel an outbound transmission request.

The Handle parameter is the returned handle from a successful Transmit Mesh
Packet request. If Zero is specified as the handle, all outstanding send
requests are canceled.

For each mesh packet canceled, the Mesh Packet Transmission Complete event will
be generated, regardless of whether the packet was sent successfully.

Possible errors:

:Failed:
:Invalid Parameters:

Send HCI command and wait for event
```````````````````````````````````

:Command Code:		0x005B
:Controller Index:	<controller id>
:Command Parameters:	Opcode (2 Octets)
:...:			Event (1 Octet)
:...:			Timeout (1 Octet)
:...:			Parameter Length (2 Octets)
:...:			Parameter[] (variable)
:Return Parameters:	Response (1-variable Octets)

This command may be used to send a HCI command and wait for an (optional) event.

The HCI command is specified by the Opcode, any arbitrary is supported including
vendor commands, but contrary to the like of Raw/User channel it is run as an
HCI command send by the kernel since it uses its command synchronization thus it
is possible to wait for a specific event as a response.

Setting event to 0x00 will cause the command to wait for either HCI Command
Status or HCI Command Complete.

Timeout is specified in seconds, setting it to 0 will cause the default timeout
to be used.

Possible errors:

:Failed:
:Invalid Parameters:

Events
------

Command Complete
````````````````

:Event Code:		0x0001
:Controller Index:	<controller id> or <non-controller>
:Event Parameters:	Command_Opcode (2 Octets)
:...:			Status (1 Octet)
:...:			Return_Parameters[] (variable)

This event is an indication that a command has completed. The fixed set of
parameters includes the opcode to identify the command that completed as well as
a status value to indicate success or failure. The rest of the parameters are
command specific and documented in the section for each command separately.

Command Status
``````````````

:Event Code:		0x0002
:Controller Index:	<controller id> or <non-controller>
:Event Parameters:	Command_Opcode (2 Octets)
:...:			Status (1 Octet)

The command status event is used to indicate an early status for a pending
command. In the case that the status indicates failure (anything else except
success status) this also means that the command has finished executing.

Controller Error
````````````````

:Event Code:		0x0003
:Controller Index:	<controller id>
:Event Parameters:	Error_Code (1 Octet)

This event maps straight to the HCI Hardware Error event and is used to indicate
something wrong with the controller hardware.

Index Added
```````````

:Event Code:		0x0004
:Controller Index:	<controller id>
:Event Parameters:

This event indicates that a new controller has been added to the system. It is
usually followed by a Read Controller Information command.

Once the Read Extended Controller Index List command has been used at least
once, the Extended Index Added event will be send instead of this one.

Index Removed
`````````````

:Event Code:		0x0005
:Controller Index:	<controller id>
:Event Parameters:

This event indicates that a controller has been removed from the system.

Once the Read Extended Controller Index List command has been used at least
once, the Extended Index Removed event will be send instead of this one.

New Settings
````````````

:Event Code:		0x0006
:Controller Index:	<controller id>
:Event Parameters:	Current_Settings (4 Octets)

This event indicates that one or more of the settings for a controller has
changed.

Class Of Device Changed
```````````````````````

:Event Code:		0x0007
:Controller Index:	<controller id>
:Event Parameters:	Class_Of_Device (3 Octets)

This event indicates that the Class of Device value for the controller has
changed. When the controller is powered off the Class of Device value will
always be reported as zero.

Local Name Changed
``````````````````

:Event Code:		0x0008
:Controller Index:	<controller id>
:Event Parameters:	Name (249 Octets)
:...:			Short_Name (11 Octets)

This event indicates that the local name of the controller has changed.

New Link Key
````````````

:Event Code:		0x0009
:Controller Index:	<controller id>
:Event Parameters:	Store_Hint (1 Octet)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Key_Type (1 Octet)
:...:			Value (16 Octets)
:...:			PIN_Length (1 Octet)

This event indicates that a new link key has been generated for a remote device.

The Store_Hint parameter indicates whether the host is expected to store the key
persistently or not (e.g. this would not be set if the authentication
requirement was "No Bonding").

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, BR/EDR
	1, Reserved (not in use)
	2, Reserved (not in use)

Public and random LE addresses are not valid and will be rejected.

Currently defined Key_Type values are:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Combination key
	0x01, Local Unit key
	0x02, Remote Unit key
	0x03, Debug Combination key
	0x04, Unauthenticated Combination key from P-192
	0x05, Authenticated Combination key from P-192
	0x06, Changed Combination key
	0x07, Unauthenticated Combination key from P-256
	0x08, Authenticated Combination key from P-256

Receiving this event indicates that a pairing procedure has been completed.

New Long Term Key
`````````````````

:Event Code:		0x000A
:Controller Index:	<controller id>
:Event Parameters:	Store_Hint (1 Octet)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Key_Type (1 Octet)
:...:			Central (1 Octet)
:...:			Encryption Size (1 Octet)
:...:			Enc. Diversifier (2 Octets)
:...:			Random Number (8 Octets)
:...:			Value (16 Octets)

This event indicates that a new long term key has been generated for a remote
device.

The Store_Hint parameter indicates whether the host is expected to store the key
persistently or not (e.g. this would not be set if the authentication
requirement was "No Bonding").

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Reserved (not in use)
	1, LE Public
	2, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

For unresolvable random addresses and resolvable random addresses without
identity information and identity resolving key, the Store_Hint will be set to
not store the long term key.

Currently defined Key_Type values are:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Unauthenticated legacy key
	0x01, Authenticated legacy key
	0x02, Unauthenticated key from P-256
	0x03, Authenticated key from P-256
	0x04, Debug key from P-256

Receiving this event indicates that a pairing procedure has been completed.

Device Connected
````````````````

:Event Code:		0x000B
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Flags (4 Octets)
:...:			EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This event indicates that a successful baseband connection has been created to
the remote device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

For devices using resolvable random addresses with a known identity resolving
key, the Address and Address_Type will contain the identity information.

It is possible that devices get connected via its resolvable random address and
after New Identity Resolving Key event start using its identity.

The following bits are defined for the Flags parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Reserved (not in use)
	1, Legacy Pairing
	2, Reserved (not in use)
	3, Initiated Connection
	4, Reserved (not in use)

Device Disconnected
```````````````````

:Event Code:		0x000C
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Reason (1 Octet)

This event indicates that the baseband connection was lost to a remote device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For devices using resolvable random addresses with a known identity resolving
key, the Address and Address_Type will contain the identity information.

Possible values for the Reason parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Unspecified
	0x01, Connection timeout
	0x02, Connection terminated by local host
	0x03, Connection terminated by remote host
	0x04, Connection terminated due to authentication failure
	0x05, Connection terminated by local host for suspend

Note that the local/remote distinction just determines which side terminated the
low-level connection, regardless of the disconnection of the higher-level
profiles.

This can sometimes be misleading and thus must be used with care.
For example, some hardware combinations would report a locally initiated
disconnection even if the user turned Bluetooth off in the remote side.

Connect Failed
``````````````

:Event Code:		0x000D
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Status (1 Octet)

This event indicates that a connection attempt failed to a remote device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For devices using resolvable random addresses with a known identity resolving
key, the Address and Address_Type will contain the identity information.

PIN Code Request
````````````````

:Event Code:		0x000E
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Secure (1 Octet)

This event is used to request a PIN Code reply from user space.
The reply should either be returned using the PIN Code Reply or the PIN Code
Negative Reply command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

:Secure: 0x01  secure PIN code required
	 0x00  secure PIN code not required

User Confirmation Request
`````````````````````````

:Event Code:		0x000F
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Confirm_Hint (1 Octet)
:...:			Value (4 Octets)

This event is used to request a user confirmation request from user space.

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

If the Confirm_Hint parameter value is 0x01 this means that a simple "Yes/No"
confirmation should be presented to the user instead of a full numerical
confirmation (in which case the parameter value will be 0x00).

User space should respond to this command either using the User Confirmation
Reply or the User Confirmation Negative Reply command.

User Passkey Request
````````````````````

:Event Code:		0x0010
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This event is used to request a passkey from user space. The response to this
event should either be the User Passkey Reply command or the User Passkey
Negative Reply command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

Authentication Failed
`````````````````````

:Event Code:		0x0011
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Status (1 Octet)

This event indicates that there was an authentication failure with a remote
device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

Device Found
````````````

:Event Code:		0x0012
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			RSSI (1 Octet)
:...:			Flags (4 Octets)
:...:			EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This event indicates that a device was found during device discovery.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The following bits are defined for the Flags parameter:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, Confirm name
	1, Legacy Pairing
	2, Not Connectable
	3, Reserved (not in use)
	4, Name Request Failed
	5, Scan Response

For the RSSI field a value of 127 indicates that the RSSI is not available. That
can happen with Bluetooth 1.1 and earlier controllers or with bad radio
conditions.

The Confirm name flag indicates that the kernel wants to know whether user space
knows the name for this device or not. If this flag is set user space should
respond to it using the Confirm Name command.

The Legacy Pairing flag indicates that Legacy Pairing is likely to occur when
pairing with this device. An application could use this information to optimize
the pairing process by locally pre-generating a PIN code and thereby eliminate
the risk of local input timeout when pairing. Note that there is a risk of
false-positives for this flag so user space should be able to handle getting
something else as a PIN Request when pairing.

The Not Connectable flag indicates that the device will not accept any
connections. This can be indicated by Low Energy devices that are in broadcaster
role.

The Name Request Failed flag indicates that name resolving procedure has ended
with failure for this device. The user space should use this information to
determine when is a good time to retry the name resolving procedure.

Discovering
```````````

:Event Code:		0x0013
:Controller Index:	<controller id>
:Event Parameters:	Address_Type (1 Octet)
:...:			Discovering (1 Octet)

This event indicates that the controller has started discovering devices. This
discovering state can come and go multiple times between a Start Discovery and a
Stop Discovery commands.

The Start Service Discovery command will also trigger this event.

The valid values for the Discovering parameter are 0x01 (enabled) and 0x00
(disabled).

Device Blocked
``````````````

:Event Code:		0x0014
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This event indicates that a device has been blocked using the Block Device
command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The event will only be sent to Management sockets other than the one through
which the command was sent.

Device Unblocked
````````````````

:Event Code:		0x0015
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This event indicates that a device has been unblocked using the Unblock Device
command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The event will only be sent to Management sockets other than the one through
which the command was sent.

Device Unpaired
```````````````

:Event Code:		0x0016
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This event indicates that a device has been unpaired (i.e. all its keys have
been removed from the kernel) using the Unpair Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For devices using resolvable random addresses with a known identity resolving
key, the event parameters will contain the identity. After receiving this event,
the device will become essentially private again.

The event will only be sent to Management sockets other than the one through
which the Unpair Device command was sent.

Passkey Notify
``````````````

:Event Code:		0x0017
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Passkey (4 Octets)
:...:			Entered (1 Octet)

This event is used to request passkey notification to the user.
Unlike the other authentication events it does not need responding to using any
Management command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The Passkey parameter indicates the passkey to be shown to the user whereas the
Entered parameter indicates how many characters the user has entered on the
remote side.

New Identity Resolving Key (since 1.5)
``````````````````````````````````````

:Event Code:		0x0018
:Controller Index:	<controller id>
:Event Parameters:	Store_Hint (1 Octet)
:...:			Random_Address (6 Octets)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Value (16 Octets)

This event indicates that a new identity resolving key has been generated for a
remote device.

The Store_Hint parameter indicates whether the host is expected to store the key
persistently or not.

The Random_Address provides the resolvable random address that was resolved into
an identity. A value of 00:00:00:00:00:00 indicates that the identity resolving
key was provided for a public address or static random address.

Once this event has been send for a resolvable random address, all further
events mapping this device will send out using the identity address information.

This event also indicates that now the identity address should be used for
commands instead of the resolvable random address.

It is possible that some devices allow discovering via its identity address, but
after pairing using resolvable private address only. In such a case Store_Hint
will be 0x00 and the Random_Address will indicate 00:00:00:00:00:00. For these
devices, the Privacy Characteristic of the remote GATT database should be
consulted to decide if the identity resolving key must be stored persistently or
not.

Devices using Set Privacy command with the option 0x02 would be such type of
device.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Reserved (not in use)
	1, LE Public
	2, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

New Signature Resolving Key (since 1.5)
```````````````````````````````````````

:Event Code:		0x0019
:Controller Index:	<controller id>
:Event Parameters:	Store_Hint (1 Octet)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Type (1 Octet)
:...:			Value (16 Octets)

This event indicates that a new signature resolving key has been generated for
either the central or peripheral device.

The Store_Hint parameter indicates whether the host is expected to store the key
persistently or not.

The Type parameter has the following possible values:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Unauthenticated local CSRK
	0x01, Unauthenticated remote CSRK
	0x02, Authenticated local CSRK
	0x03, Authenticated remote CSRK

The local keys are used for signing data to be sent to the remote device,
whereas the remote keys are used to verify signatures received from the remote
device.

The local signature resolving key will be generated with each pairing request.
Only after receiving this event with the Type indicating a local key is it
possible to use ATT Signed Write procedures.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Reserved (not in use)
	0x01, LE Public
	0x02, LE Random

The provided Address and Address_Type are the identity of a device. So either
its public address or static random address.

Device Added (since 1.7)
````````````````````````

:Event Code:		0x001a
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Action (1 Octet)

This event indicates that a device has been added using the Add Device command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The event will only be sent to management sockets other than the one through
which the command was sent.

Device Removed (since 1.7)
``````````````````````````

:Event Code:		0x001b
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)

This event indicates that a device has been removed using the Remove Device
command.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The event will only be sent to management sockets other than the one through
which the command was sent.

New Connection Parameter (since 1.7)
````````````````````````````````````

:Event Code:		0x001c
:Controller Index:	<controller id>
:Event Parameters:	Store_Hint (1 Octet)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Min_Connection_Interval (2 Octets)
:...:			Max_Connection_Interval (2 Octets)
:...:			Connection_Latency (2 Octets)
:...:			Supervision_Timeout (2 Octets)

This event indicates a new set of connection parameters from a peripheral
device.

The Store_Hint parameter indicates whether the host is expected to store this
information persistently or not.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Reserved (not in use)
	0x01, LE Public
	0x02, LE Random

The Min_Connection_Interval, Max_Connection_Interval, Connection_Latency and
Supervision_Timeout parameters are encoded as described in Core 4.1 spec, Vol 2,
7.7.65.3.

Unconfigured Index Added (since 1.7)
````````````````````````````````````

:Event Code:		0x001d
:Controller Index:	<controller id>
:Event Parameters:

This event indicates that a new unconfigured controller has been added to the
system. It is usually followed by a Read Controller Configuration Information
command.

Only when a controller requires further configuration, it will be announced with
this event. If it supports configuration, but does not require it, then an Index
Added event will be used.

Once the Read Extended Controller Index List command has been used at least
once, the Extended Index Added event will be send instead of this one.

Unconfigured Index Removed (since 1.7)
``````````````````````````````````````

:Event Code:		0x001e
:Controller Index:	<controller id>
:Event Parameters:

This event indicates that an unconfigured controller has been removed from the
system.

Once the Read Extended Controller Index List command has been used at least
once, the Extended Index Removed event will be send instead of this one.

New Configuration Options (since 1.7)
`````````````````````````````````````

:Event Code:		0x001f
:Controller Index:	<controller id>
:Event Parameters:	Missing_Options (4 Octets)

This event indicates that one or more of the options for the controller
configuration has changed.

Extended Index Added (since 1.9)
````````````````````````````````

:Event Code:		0x0020
:Controller Index:	<controller id>
:Event Parameters:	Controller_Type (1 Octet)
:...:			Controller_Bus (1 Octet)

This event indicates that a new controller index has been added to the system.

This event will only be used after Read Extended Controller Index List has been
used at least once. If it has not been used, then Index Added and Unconfigured
Index Added are sent instead.

Extended Index Removed (since 1.9)
``````````````````````````````````

:Event Code:		0x0021
:Controller Index:	<controller id>
:Event Parameters:	Controller_Type (1 Octet)
:...:			Controller_Bus (1 Octet)

This event indicates that an existing controller index has been removed from the
system.

This event will only be used after Read Extended Controller Index List has been
used at least once. If it has not been used, then Index Removed and Unconfigured
Index Removed are sent instead.

Local Out Of Band Extended Data Updated (since 1.9)
```````````````````````````````````````````````````

:Event Code:		0x0022
:Controller Index:	<controller id>
:Event Parameters:	Address_Type (1 Octet)
:...:			EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This event is used when the Read Local Out Of Band Extended Data command has
been used and some other user requested a new set of local out-of-band data.
This allows for the original caller to adjust the data.

Possible values for the Address_Type parameter are a bit-wise or of the
following bits:

.. csv-table::
	:header: "Bit", "Description"
	:widths: auto

	0, BR/EDR
	1, LE Public
	2, LE Random

By combining these e.g. the following values are possible:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x01, BR/EDR
	0x06, LE (public & random)
	0x07, Reserved (not in use)

The value for EIR_Data_Length and content for EIR_Data is the same as described
in Read Local Out Of Band Extended Data command.

When LE Privacy is used and LE Secure Connections out-of-band data has been
requested, then this event will be emitted every time the Resolvable Private
Address (RPA) gets changed. The new RPA will be included in the EIR_Data.

The event will only be sent to management sockets other than the one through
which the command was sent. It will additionally also only be sent to sockets
that have used the command at least once.

Advertising Added (since 1.9)
`````````````````````````````

:Event Code:		0x0023
:Controller Index:	<controller id>
:Event Parameters:	Instance (1 Octet)

This event indicates that an advertising instance has been added using the Add
Advertising command.

The event will only be sent to management sockets other than the one through
which the command was sent.

Advertising Removed (since 1.9)
```````````````````````````````

:Event Code:		0x0024
:Controller Index:	<controller id>
:Event Parameters:	Instance (1 Octet)

This event indicates that an advertising instance has been removed using the
Remove Advertising command.

The event will only be sent to management sockets other than the one through
which the command was sent.

Extended Controller Information Changed (since 1.14)
````````````````````````````````````````````````````

:Event Code:		0x0025
:Controller Index:	<controller id>
:Event Parameters:	EIR_Data_Length (2 Octets)
:...:			EIR_Data (0-65535 Octets)

This event indicates that controller information has been updated and new values
are used. This includes the local name, class of device, device id and LE
address information.

This event will only be used after Read Extended Controller Information command
has been used at least once. If it has not been used the legacy events are used.

The event will only be sent to management sockets other than the one through
which the change was triggered.

PHY Configuration Changed (since 1.14)
``````````````````````````````````````

:Event Code:		0x0026
:Controller Index:	<controller id>
:Event Parameters:	Selected_PHYs (4 Octets)

This event indicates that default PHYs have been updated.

This event will only be used after Set PHY Configuration command has been used
at least once.

The event will only be sent to management sockets other than the one through
which the change was triggered.

Refer Get PHY Configuration command for PHYs parameter.

Experimental Feature Changed (since 1.17)
`````````````````````````````````````````

:Event Code:		0x0027
:Controller Index:	<controller id>
:Event Parameters:	UUID (16 Octets)
:...:			Flags (4 Octets)

This event indicates that the status of an experimental feature has been
changed.

The event will only be sent to management sockets other than the one through
which the change was triggered.

Refer to Set Experimental Feature command for the Flags parameter.

Default System Configuration Changed (since 1.18)
`````````````````````````````````````````````````

:Event Code:		0x0028
:Controller Index:	<controller id>
:Event Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value[] (0-255 Octets)

This event indicates the change of default system parameter values.

The event will only be sent to management sockets other than the one through
which the change was triggered. In addition it will only be sent to sockets that
have issues the Read Default System Configuration command.

Refer to Read Default System configuration command for the supported
Parameter_Type values.

Default Runtime Configuration Changed (since 1.18)
``````````````````````````````````````````````````

:Event Code:		0x0029
:Controller Index:	<controller id>
:Event Parameters:	Parameter_Type[] (2 Octet)
:...:			Value_Length[] (1 Octet)
:...:			Value[] (0-255 Octets)

This event indicates the change of default runtime parameter values.

The event will only be sent to management sockets other than the one through
which the change was triggered. In addition it will only be sent to sockets that
have issues the Read Default Runtime Configuration command.

Refer to Read Default Runtime configuration command for the supported
Parameter_Type values.

Device Flags Changed (since 1.18)
`````````````````````````````````

:Event Code:		0x002a
:Controller Index:	<controller id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			Supported_Flags (4 Octets)
:...:			Current_Flags (4 Octets)

This event indicates that the device flags have been changed via the Set Device
Flags command or that a new device has been added via the Add Device command. In
the latter case it is send right after the Device Added event.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

The event will only be sent to management sockets other than the one through
which the command was sent.

In case this event is triggered by Add Device then it is sent to all management
sockets.

Advertisement Monitor Added (since 1.18)
````````````````````````````````````````

:Event Code:		0x002b
:Controller Index:	<controller id>
:Event Parameters:	Monitor_Handle (2 Octets)

This event indicates that an advertisement monitor has been added using the Add
Advertisement Patterns Monitor command.

The event will only be sent to management sockets other than the one through
which the command was sent.

Advertisement Monitor Removed (since 1.18)
``````````````````````````````````````````

:Event Code:		0x002c
:Controller Index:	<controller id>
:Event Parameters:	Monitor_Handle (2 Octets)

This event indicates that an advertisement monitor has been removed using the
Remove Advertisement Monitor command.

The event will only be sent to management sockets other than the one through
which the command was sent.

Controller Suspend (since 1.18)
```````````````````````````````

:Event Code:		0x002d
:Controller Index:	<controller_id>
:Event Parameters:	Suspend_State (1 octet)

This event indicates that the controller is suspended for host suspend.

Possible values for the Suspend_State parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Running (not disconnected)
	0x01, Disconnected and not scanning
	0x02, Page scanning and/or passive scanning.

The value 0x00 is used for the running state and may be sent if the controller
could not be configured to suspend properly.

This event will be sent to all management sockets.

Controller Resume (since 1.18)
``````````````````````````````

:Event Code:		0x002e
:Controller Index:	<controller_id>
:Event Parameters:	Wake_Reason (1 octet)
:...:			Address (6 octets)
:...:			Address_Type (1 octet)

This event indicates that the controller has resumed from suspend.

Possible values for the Wake_Reason parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, Resume from non-Bluetooth wake source
	0x01, Wake due to unexpected event
	0x02, Remote wake due to peer device connection

Currently, we expect that only peer reconnections should wake us from the
suspended state. Any other events that occurred while the system should have
been suspended results in wake due to unexpected event.

If the Wake_Reason is Remote wake due to connection, the address of the peer
device that caused the event will be shared in Address and Address_Type.
Otherwise, Address and Address_Type will both be zero.

This event will be sent to all management sockets.

Advertisement Monitor Device Found (since 1.18)
```````````````````````````````````````````````

:Event Code:		0x002f
:Controller Index:	<controller_id>
:Event Parameters:	Monitor_Handle (2 Octets)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			RSSI (1 Octet)
:...:			Flags (4 Octets)
:...:			AD_Data_Length (2 Octets)
:...:			AD_Data (0-65535 Octets)

This event indicates that the controller has started tracking a device matching
an Advertisement Monitor with handle Monitor_Handle.

Monitor_Handle 0 indicates that we are not active scanning and this is a
subsequent advertisement report for already matched Advertisement Monitor or the
controller offloading support is not available so need to report all
advertisements for software based filtering.

The address of the device being tracked will be shared in Address and
Address_Type.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0x00, BR/EDR
	0x01, LE Public
	0x02, LE Random

For the RSSI field a value of 127 indicates that the RSSI is not available. That
can happen with Bluetooth 1.1 and earlier controllers or with bad radio
conditions.

This event will be sent to all management sockets.

Advertisement Monitor Device Lost (since 1.18)
``````````````````````````````````````````````

:Event Code:		0x0030
:Controller Index:	<controller_id>
:Event Parameters:	Monitor_Handle (2 Octets)
:...:			Address (6 Octets)
:...:			Address_Type (1 Octet)

This event indicates that the controller has stopped tracking a device that was
being tracked by an Advertisement Monitor with the handle Monitor_Handle.

The address of the device being tracked will be shared in Address and
Address_Type.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Reserved (not in use)
	1, LE Public
	2, LE Random

This event will be sent to all management sockets.

Mesh Device Found (since 1.21)
``````````````````````````````

:Event Code:		0x0031
:Controller Index:	<controller_id>
:Event Parameters:	Address (6 Octets)
:...:			Address_Type (1 Octet)
:...:			RSSI (1 Octet)
:...:			Instant (8 Octets)
:...:			Flags (4 Octets)
:...:			AD_Data_Length (2 Octets)
:...:			AD_Data (0-65535 Octets)

This event indicates that the controller has received an Advertisement or Scan
Result containing an AD Type matching the Mesh scan set.

The address of the sending device is returned, and must be a valid LE
Address_Type.

Possible values for the Address_Type parameter:

.. csv-table::
	:header: "Value", "Description"
	:widths: auto

	0, Reserved (not in use)
	1, LE Public
	2, LE Random

The RSSI field is a signed octet, and is the RSSI reported by the receiving
controller.

The Instant field is 64 bit value that represents the instant in time the packet
was received. It's value is not intended to be interpreted by the host, and is
only useful if the host wants to make a timed response to the received packet
(i.e. a Poll/Response).

AD_Length and AD_Data contains the Info structure of Advertising and Scan
rsults. To receive this event, AD filters must be requested with the Set Mesh
Receiver command, specifying which AD Types to return. All AD structures
will be received in this event if any of the filtered AD Types are present.

This event will be sent to all management sockets.

Mesh Packet Transmit Complete (since 1.21)
``````````````````````````````````````````

:Event Code:		0x0032
:Controller Index:	<controller_id>
:Event Parameters:	Handle (1 Octets)

This event indicates that a requested outbound Mesh packet has completed and no
longer occupies a transmit slot.

This event will be sent to all management sockets.
