/*
* PROJECT:     ReactOS Universal Audio Class Driver
* LICENSE:     GPL - See COPYING in the top level directory
* FILE:        drivers/usb/usbaudio/filter.c
* PURPOSE:     USB Audio device driver.
* PROGRAMMERS:
*              Johannes Anderwald (johannes.anderwald@reactos.org)
*/

#include "usbaudio.h"

GUID NodeTypeMicrophone = { STATIC_KSNODETYPE_MICROPHONE };
GUID NodeTypeDesktopMicrophone = { STATIC_KSNODETYPE_DESKTOP_MICROPHONE };
GUID NodeTypePersonalMicrophone = { STATIC_KSNODETYPE_PERSONAL_MICROPHONE };
GUID NodeTypeOmmniMicrophone = { STATIC_KSNODETYPE_OMNI_DIRECTIONAL_MICROPHONE };
GUID NodeTypeArrayMicrophone = { STATIC_KSNODETYPE_MICROPHONE_ARRAY };
GUID NodeTypeProcessingArrayMicrophone = { STATIC_KSNODETYPE_PROCESSING_MICROPHONE_ARRAY };
GUID NodeTypeSpeaker = { STATIC_KSNODETYPE_SPEAKER };
GUID NodeTypeHeadphonesSpeaker = { STATIC_KSNODETYPE_HEADPHONES };
GUID NodeTypeHMDA = { STATIC_KSNODETYPE_HEAD_MOUNTED_DISPLAY_AUDIO };
GUID NodeTypeDesktopSpeaker = { STATIC_KSNODETYPE_DESKTOP_SPEAKER };
GUID NodeTypeRoomSpeaker = { STATIC_KSNODETYPE_ROOM_SPEAKER };
GUID NodeTypeCommunicationSpeaker = { STATIC_KSNODETYPE_COMMUNICATION_SPEAKER };
GUID NodeTypeSubwoofer = { STATIC_KSNODETYPE_LOW_FREQUENCY_EFFECTS_SPEAKER };
GUID NodeTypeCapture = { STATIC_PINNAME_CAPTURE };
GUID NodeTypePlayback = { STATIC_KSCATEGORY_AUDIO };
GUID GUID_KSCATEGORY_AUDIO = { STATIC_KSCATEGORY_AUDIO };

KSPIN_INTERFACE StandardPinInterface =
{
     {STATIC_KSINTERFACESETID_Standard},
     KSINTERFACE_STANDARD_STREAMING,
     0
};

KSPIN_MEDIUM StandardPinMedium =
{
     {STATIC_KSMEDIUMSETID_Standard},
     KSMEDIUM_TYPE_ANYINSTANCE,
     0
};

KSDATARANGE BridgePinAudioFormat[] =
{
    {
        {
            sizeof(KSDATAFORMAT),
            0,
            0,
            0,
            {STATIC_KSDATAFORMAT_TYPE_AUDIO},
            {STATIC_KSDATAFORMAT_SUBTYPE_ANALOG},
            {STATIC_KSDATAFORMAT_SPECIFIER_NONE}
        }
    }
};

static PKSDATARANGE BridgePinAudioFormats[] =
{
    &BridgePinAudioFormat[0]
};

static LPWSTR ReferenceString = L"global";

NTSTATUS
NTAPI
USBAudioFilterCreate(
    PKSFILTER Filter,
    PIRP Irp);

static KSFILTER_DISPATCH USBAudioFilterDispatch = 
{
    USBAudioFilterCreate,
    NULL,
    NULL,
    NULL
};

static KSPIN_DISPATCH UsbAudioPinDispatch =
{
    USBAudioPinCreate,
    USBAudioPinClose,
    USBAudioPinProcess,
    USBAudioPinReset,
    USBAudioPinSetDataFormat,
    USBAudioPinSetDeviceState,
    NULL,
    NULL,
    NULL,
    NULL
};

ULONG
CountTopologyComponents(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor)
{
    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR InterfaceHeaderDescriptor;
    PUSB_COMMON_DESCRIPTOR CommonDescriptor;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR InputTerminalDescriptor;
    PUSB_AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR FeatureUnitDescriptor;
    PUSB_AUDIO_CONTROL_MIXER_UNIT_DESCRIPTOR MixerUnitDescriptor;
    ULONG NodeCount = 0;
    UCHAR Value;

    for (Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
    Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x01) /* AUDIO_CONTROL */
        {
            InterfaceHeaderDescriptor = (PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR)USBD_ParseDescriptors(ConfigurationDescriptor, ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (InterfaceHeaderDescriptor != NULL)
            {
                CommonDescriptor = USBD_ParseDescriptors(InterfaceHeaderDescriptor, InterfaceHeaderDescriptor->wTotalLength, (PVOID)((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->bLength), USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
                while (CommonDescriptor)
                {
                    InputTerminalDescriptor = (PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR)CommonDescriptor;
                    if (InputTerminalDescriptor->bDescriptorSubtype == 0x02 /* INPUT TERMINAL*/ || InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        NodeCount++;
                    }
                    else if (InputTerminalDescriptor->bDescriptorSubtype == 0x06 /* FEATURE_UNIT*/)
                    {
                        FeatureUnitDescriptor = (PUSB_AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR)InputTerminalDescriptor;
                        Value = FeatureUnitDescriptor->bmaControls[0];
                        if (Value & 0x01) /* MUTE*/
                            NodeCount++;
                        if (Value & 0x02) /* VOLUME */
                            NodeCount++;
                        if (Value & 0x04) /* BASS */
                            NodeCount++;
                        if (Value & 0x08) /* MID */
                            NodeCount++;
                        if (Value & 0x10) /* TREBLE */
                            NodeCount++;
                        if (Value & 0x20) /* GRAPHIC EQUALIZER */
                            NodeCount++;
                        if (Value & 0x40) /* AUTOMATIC GAIN */
                            NodeCount++;
                        if (Value & 0x80) /* DELAY */
                            NodeCount++;

                        /* FIXME handle logical channels too */
                    }
                    else if (InputTerminalDescriptor->bDescriptorSubtype == 0x04 /* MIXER_UNIT */)
                    {
                        MixerUnitDescriptor = (PUSB_AUDIO_CONTROL_MIXER_UNIT_DESCRIPTOR)InputTerminalDescriptor;
                        NodeCount += MixerUnitDescriptor->bNrInPins + 1; /* KSNODETYPE_SUPERMIX for each source pin and KSNODETYPE_SUM for target */
                    }
                    else
                    {
                        UNIMPLEMENTED
                    }
                    CommonDescriptor = (PUSB_COMMON_DESCRIPTOR)((ULONG_PTR)CommonDescriptor + CommonDescriptor->bLength);
                    if ((ULONG_PTR)CommonDescriptor >= ((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->wTotalLength))
                        break;
                }
            }
        }
    }
    return NodeCount;
}



NTSTATUS
BuildUSBAudioFilterTopology(
    PKSDEVICE Device,
    PKSFILTER_DESCRIPTOR FilterDescriptor)
{
    PDEVICE_EXTENSION DeviceExtension;
    ULONG NodeCount, Index;
    UCHAR Value;
    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR InterfaceHeaderDescriptor;
    PUSB_COMMON_DESCRIPTOR CommonDescriptor;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR InputTerminalDescriptor;
    PUSB_AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR FeatureUnitDescriptor;
    PUSB_AUDIO_CONTROL_MIXER_UNIT_DESCRIPTOR MixerUnitDescriptor;
    PKSNODE_DESCRIPTOR NodeDescriptors;

    /* get device extension */
    DeviceExtension = Device->Context;

    /* count topology nodes */
    NodeCount = CountTopologyComponents(DeviceExtension->ConfigurationDescriptor);

    /* init node descriptors*/
    FilterDescriptor->NodeDescriptors = NodeDescriptors = AllocFunction(NodeCount * sizeof(KSNODE_DESCRIPTOR));
    if (FilterDescriptor->NodeDescriptors == NULL)
    {
        /* no memory */
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    FilterDescriptor->NodeDescriptorSize = sizeof(KSNODE_DESCRIPTOR);

    for (Descriptor = USBD_ParseConfigurationDescriptorEx(DeviceExtension->ConfigurationDescriptor, DeviceExtension->ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
    Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(DeviceExtension->ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x01) /* AUDIO_CONTROL */
        {
            InterfaceHeaderDescriptor = (PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR)USBD_ParseDescriptors(DeviceExtension->ConfigurationDescriptor, DeviceExtension->ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (InterfaceHeaderDescriptor != NULL)
            {
                CommonDescriptor = USBD_ParseDescriptors(InterfaceHeaderDescriptor, InterfaceHeaderDescriptor->wTotalLength, (PVOID)((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->bLength), USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
                while (CommonDescriptor)
                {
                    InputTerminalDescriptor = (PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR)CommonDescriptor;
                    if (InputTerminalDescriptor->bDescriptorSubtype == 0x02 /* INPUT TERMINAL*/ || InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        if (InputTerminalDescriptor->wTerminalType == USB_AUDIO_STREAMING_TERMINAL_TYPE)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_SRC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_SRC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        else if ((InputTerminalDescriptor->wTerminalType & 0xFF00) == 0x200)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_ADC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_ADC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        else if ((InputTerminalDescriptor->wTerminalType & 0xFF00) == 0x300)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_DAC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_DAC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        else
                        {
                            DPRINT1("Unexpected input terminal type %x\n", InputTerminalDescriptor->wTerminalType);
                        }
                    }
                    else if (InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        if (InputTerminalDescriptor->wTerminalType == USB_AUDIO_STREAMING_TERMINAL_TYPE)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_SRC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_SRC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        else if ((InputTerminalDescriptor->wTerminalType & 0xFF00) == 0x300)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_DAC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_DAC;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        else
                        {
                            DPRINT1("Unexpected output terminal type %x\n", InputTerminalDescriptor->wTerminalType);
                        }
                    }

                    else if (InputTerminalDescriptor->bDescriptorSubtype == 0x06 /* FEATURE_UNIT*/)
                    {
                        FeatureUnitDescriptor = (PUSB_AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR)InputTerminalDescriptor;
                        Value = FeatureUnitDescriptor->bmaControls[0];
                        if (Value & 0x01) /* MUTE*/
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_MUTE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_MUTE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                        if (Value & 0x02) /* VOLUME */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_VOLUME;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_VOLUME;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x04) /* BASS */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x08) /* MID */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x10) /* TREBLE */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x20) /* GRAPHIC EQUALIZER */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x40) /* AUTOMATIC GAIN */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        if (Value & 0x80) /* DELAY */
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_TONE;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }
                    }
                    else if (InputTerminalDescriptor->bDescriptorSubtype == 0x04 /* MIXER_UNIT */)
                    {
                        MixerUnitDescriptor = (PUSB_AUDIO_CONTROL_MIXER_UNIT_DESCRIPTOR)InputTerminalDescriptor;
                        for (Index = 0; Index < MixerUnitDescriptor->bNrInPins; Index++)
                        {
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_SUPERMIX;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_SUPERMIX;
                            NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                            FilterDescriptor->NodeDescriptorsCount++;
                        }

                        NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Type = &KSNODETYPE_SUM;
                        NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].Name = &KSNODETYPE_SUM;
                        NodeDescriptors[FilterDescriptor->NodeDescriptorsCount].AutomationTable = AllocFunction(sizeof(KSAUTOMATION_TABLE));
                        FilterDescriptor->NodeDescriptorsCount++;
                    }
                    else
                    {
                        UNIMPLEMENTED
                    }
                    CommonDescriptor = (PUSB_COMMON_DESCRIPTOR)((ULONG_PTR)CommonDescriptor + CommonDescriptor->bLength);
                    if ((ULONG_PTR)CommonDescriptor >= ((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->wTotalLength))
                        break;
                }
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
USBAudioFilterCreate(
    PKSFILTER Filter,
    PIRP Irp)
{
    PKSFILTERFACTORY FilterFactory;
    PKSDEVICE Device;
    PFILTER_CONTEXT FilterContext;

    FilterFactory = KsGetParent(Filter);
    if (FilterFactory == NULL)
    {
        /* invalid parameter */
        return STATUS_INVALID_PARAMETER;
    }

    Device = KsGetParent(FilterFactory);
    if (Device == NULL)
    {
        /* invalid parameter */
        return STATUS_INVALID_PARAMETER;
    }

    /* alloc filter context */
    FilterContext = AllocFunction(sizeof(FILTER_CONTEXT));
    if (FilterContext == NULL)
    {
        /* no memory */
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* init context */
    FilterContext->DeviceExtension = Device->Context;
    FilterContext->LowerDevice = Device->NextDeviceObject;
    Filter->Context = FilterContext;

    DPRINT("USBAudioFilterCreate FilterContext %p LowerDevice %p DeviceExtension %p\n", FilterContext, FilterContext->LowerDevice, FilterContext->DeviceExtension);
    KsAddItemToObjectBag(Filter->Bag, FilterContext, ExFreePool);
    return STATUS_SUCCESS;
}


VOID
CountTerminalUnits(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    OUT PULONG NonStreamingTerminalDescriptorCount,
    OUT PULONG TotalTerminalDescriptorCount)
{
    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR InterfaceHeaderDescriptor;
    PUSB_COMMON_DESCRIPTOR CommonDescriptor;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR InputTerminalDescriptor;
    ULONG NonStreamingTerminalCount = 0;
    ULONG TotalTerminalCount = 0;

    for(Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
        Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x01) /* AUDIO_CONTROL */
        {
            InterfaceHeaderDescriptor = (PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR)USBD_ParseDescriptors(ConfigurationDescriptor, ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (InterfaceHeaderDescriptor != NULL)
            {
                CommonDescriptor = USBD_ParseDescriptors(InterfaceHeaderDescriptor, InterfaceHeaderDescriptor->wTotalLength, (PVOID)((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->bLength), USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
                while (CommonDescriptor)
                {
                    InputTerminalDescriptor = (PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR)CommonDescriptor;
                    if (InputTerminalDescriptor->bDescriptorSubtype == 0x02 /* INPUT TERMINAL*/ || InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        if (InputTerminalDescriptor->wTerminalType != USB_AUDIO_STREAMING_TERMINAL_TYPE)
                        {
                            NonStreamingTerminalCount++;
                        }
                        TotalTerminalCount++;
                    }
                    CommonDescriptor = (PUSB_COMMON_DESCRIPTOR)((ULONG_PTR)CommonDescriptor + CommonDescriptor->bLength);
                    if ((ULONG_PTR)CommonDescriptor >= ((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->wTotalLength))
                        break;
                }
            }
        }
        else if (Descriptor->bInterfaceSubClass == 0x03) /* MIDI_STREAMING */
        {
            UNIMPLEMENTED
        }
    }
    *NonStreamingTerminalDescriptorCount = NonStreamingTerminalCount;
    *TotalTerminalDescriptorCount = TotalTerminalCount;
}

LPGUID
UsbAudioGetPinCategoryFromTerminalDescriptor(
    IN PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR TerminalDescriptor)
{
    if (TerminalDescriptor->wTerminalType == USB_AUDIO_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypeMicrophone;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_DESKTOP_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypeDesktopMicrophone;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_PERSONAL_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypePersonalMicrophone;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_OMMNI_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypeOmmniMicrophone;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_ARRAY_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypeArrayMicrophone;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_ARRAY_PROCESSING_MICROPHONE_TERMINAL_TYPE)
        return &NodeTypeProcessingArrayMicrophone;

    /* playback types */
    if (TerminalDescriptor->wTerminalType == USB_AUDIO_SPEAKER_TERMINAL_TYPE)
        return &NodeTypeSpeaker;
    else if (TerminalDescriptor->wTerminalType == USB_HEADPHONES_SPEAKER_TERMINAL_TYPE)
        return &NodeTypeHeadphonesSpeaker;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_HMDA_TERMINAL_TYPE)
        return &NodeTypeHMDA;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_DESKTOP_SPEAKER_TERMINAL_TYPE)
        return &NodeTypeDesktopSpeaker;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_ROOM_SPEAKER_TERMINAL_TYPE)
        return &NodeTypeRoomSpeaker;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_COMMUNICATION_SPEAKER_TERMINAL_TYPE)
        return &NodeTypeCommunicationSpeaker;
    else if (TerminalDescriptor->wTerminalType == USB_AUDIO_SUBWOOFER_TERMINAL_TYPE)
        return &NodeTypeSubwoofer;

    if (TerminalDescriptor->wTerminalType == USB_AUDIO_STREAMING_TERMINAL_TYPE)
    {
        if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_OUTPUT_TERMINAL)
            return &NodeTypeCapture;
        else if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_INPUT_TERMINAL)
            return &NodeTypePlayback;

    }
    return NULL;
}

PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR
UsbAudioGetStreamingTerminalDescriptorByIndex(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    IN ULONG Index)
{
    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR InterfaceHeaderDescriptor;
    PUSB_COMMON_DESCRIPTOR CommonDescriptor;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR InputTerminalDescriptor;
    ULONG TerminalCount = 0;

    for (Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
    Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x01) /* AUDIO_CONTROL */
        {
            InterfaceHeaderDescriptor = (PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR)USBD_ParseDescriptors(ConfigurationDescriptor, ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (InterfaceHeaderDescriptor != NULL)
            {
                CommonDescriptor = USBD_ParseDescriptors(InterfaceHeaderDescriptor, InterfaceHeaderDescriptor->wTotalLength, (PVOID)((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->bLength), USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
                while (CommonDescriptor)
                {
                    InputTerminalDescriptor = (PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR)CommonDescriptor;
                    if (InputTerminalDescriptor->bDescriptorSubtype == 0x02 /* INPUT TERMINAL*/ || InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        if (InputTerminalDescriptor->wTerminalType == USB_AUDIO_STREAMING_TERMINAL_TYPE)
                        {
                            if (TerminalCount == Index)
                            {
                                return InputTerminalDescriptor;
                            }
                            TerminalCount++;
                        }
                    }
                    CommonDescriptor = (PUSB_COMMON_DESCRIPTOR)((ULONG_PTR)CommonDescriptor + CommonDescriptor->bLength);
                    if ((ULONG_PTR)CommonDescriptor >= ((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->wTotalLength))
                        break;
                }
            }
        }
    }
    return NULL;
}

PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR
UsbAudioGetNonStreamingTerminalDescriptorByIndex(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    IN ULONG Index)
{

    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR InterfaceHeaderDescriptor;
    PUSB_COMMON_DESCRIPTOR CommonDescriptor;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR InputTerminalDescriptor;
    ULONG TerminalCount = 0;

    for (Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
    Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x01) /* AUDIO_CONTROL */
        {
            InterfaceHeaderDescriptor = (PUSB_AUDIO_CONTROL_INTERFACE_HEADER_DESCRIPTOR)USBD_ParseDescriptors(ConfigurationDescriptor, ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (InterfaceHeaderDescriptor != NULL)
            {
                CommonDescriptor = USBD_ParseDescriptors(InterfaceHeaderDescriptor, InterfaceHeaderDescriptor->wTotalLength, (PVOID)((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->bLength), USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
                while (CommonDescriptor)
                {
                    InputTerminalDescriptor = (PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR)CommonDescriptor;
                    if (InputTerminalDescriptor->bDescriptorSubtype == 0x02 /* INPUT TERMINAL*/ || InputTerminalDescriptor->bDescriptorSubtype == 0x03 /* OUTPUT_TERMINAL*/)
                    {
                        if (InputTerminalDescriptor->wTerminalType != USB_AUDIO_STREAMING_TERMINAL_TYPE)
                        {
                            if (TerminalCount == Index)
                            {
                                return InputTerminalDescriptor;
                            }
                            TerminalCount++;
                        }
                    }
                    CommonDescriptor = (PUSB_COMMON_DESCRIPTOR)((ULONG_PTR)CommonDescriptor + CommonDescriptor->bLength);
                    if ((ULONG_PTR)CommonDescriptor >= ((ULONG_PTR)InterfaceHeaderDescriptor + InterfaceHeaderDescriptor->wTotalLength))
                        break;
                }
            }
        }
    }
    return NULL;
}

VOID
UsbAudioGetDataRanges(
    IN PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor,
    IN UCHAR bTerminalID,
    OUT PKSDATARANGE** OutDataRanges,
    OUT PULONG OutDataRangesCount)
{
    PUSB_AUDIO_STREAMING_INTERFACE_DESCRIPTOR StreamingInterfaceDescriptor;
    PUSB_AUDIO_STREAMING_FORMAT_TYPE_DESCRIPTOR StreamingFormatDescriptor;
    PUSB_INTERFACE_DESCRIPTOR Descriptor;
    PKSDATARANGE_AUDIO DataRangeAudio;
    PKSDATARANGE *DataRangeAudioArray;
    ULONG NumFrequency;

    for (Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, ConfigurationDescriptor, -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1);
    Descriptor != NULL;
        Descriptor = USBD_ParseConfigurationDescriptorEx(ConfigurationDescriptor, (PVOID)((ULONG_PTR)Descriptor + Descriptor->bLength), -1, -1, USB_DEVICE_CLASS_AUDIO, -1, -1))
    {
        if (Descriptor->bInterfaceSubClass == 0x02) /* AUDIO_STREAMING */
        {
            StreamingInterfaceDescriptor = (PUSB_AUDIO_STREAMING_INTERFACE_DESCRIPTOR)USBD_ParseDescriptors(ConfigurationDescriptor, ConfigurationDescriptor->wTotalLength, Descriptor, USB_AUDIO_CONTROL_TERMINAL_DESCRIPTOR_TYPE);
            if (StreamingInterfaceDescriptor != NULL)
            {
                ASSERT(StreamingInterfaceDescriptor->bDescriptorSubtype == 0x01);
                ASSERT(StreamingInterfaceDescriptor->wFormatTag == WAVE_FORMAT_PCM);
                if (StreamingInterfaceDescriptor->bTerminalLink == bTerminalID)
                {
                    StreamingFormatDescriptor = (PUSB_AUDIO_STREAMING_FORMAT_TYPE_DESCRIPTOR)((ULONG_PTR)StreamingInterfaceDescriptor + StreamingInterfaceDescriptor->bLength);
                    ASSERT(StreamingFormatDescriptor->bDescriptorType == 0x24);
                    ASSERT(StreamingFormatDescriptor->bDescriptorSubtype == 0x02);
                    ASSERT(StreamingFormatDescriptor->bFormatType == 0x01);

                    DataRangeAudio = AllocFunction(sizeof(KSDATARANGE_AUDIO));
                    if (DataRangeAudio == NULL)
                    {
                        /* no memory*/
                        return;
                    }

                    DataRangeAudio->DataRange.FormatSize = sizeof(KSDATARANGE_AUDIO);
                    DataRangeAudio->DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
                    DataRangeAudio->DataRange.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
                    DataRangeAudio->DataRange.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
                    DataRangeAudio->MaximumChannels = 1;
                    DataRangeAudio->MinimumBitsPerSample = StreamingFormatDescriptor->bBitResolution;
                    DataRangeAudio->MaximumBitsPerSample = StreamingFormatDescriptor->bBitResolution;
                    NumFrequency = StreamingFormatDescriptor->bSamFreqType - 1;
                    DataRangeAudio->MinimumSampleFrequency = StreamingFormatDescriptor->tSamFreq[0] | StreamingFormatDescriptor->tSamFreq[1] << 8 | StreamingFormatDescriptor->tSamFreq[2] << 16;
                    DataRangeAudio->MaximumSampleFrequency = StreamingFormatDescriptor->tSamFreq[NumFrequency*3] | StreamingFormatDescriptor->tSamFreq[NumFrequency * 3+1] << 8 | StreamingFormatDescriptor->tSamFreq[NumFrequency * 3+2]<<16;
                    DataRangeAudioArray = AllocFunction(sizeof(PKSDATARANGE_AUDIO));
                    if (DataRangeAudioArray == NULL)
                    {
                        /* no memory */
                        FreeFunction(DataRangeAudio);
                        return;
                    }
                    DataRangeAudioArray[0] = (PKSDATARANGE)DataRangeAudio;
                    *OutDataRanges = DataRangeAudioArray;
                    *OutDataRangesCount = 1;
                    return;
                }
            }
        }
    }
}


NTSTATUS
USBAudioPinBuildDescriptors(
    PKSDEVICE Device,
    PKSPIN_DESCRIPTOR_EX *PinDescriptors,
    PULONG PinDescriptorsCount,
    PULONG PinDescriptorSize)
{
    PDEVICE_EXTENSION DeviceExtension;
    PKSPIN_DESCRIPTOR_EX Pins;
    ULONG TotalTerminalDescriptorCount = 0;
    ULONG NonStreamingTerminalDescriptorCount = 0;
    ULONG Index = 0;
    PUSB_AUDIO_CONTROL_INPUT_TERMINAL_DESCRIPTOR TerminalDescriptor = NULL;

    /* get device extension */
    DeviceExtension = Device->Context;

    CountTerminalUnits(DeviceExtension->ConfigurationDescriptor, &NonStreamingTerminalDescriptorCount, &TotalTerminalDescriptorCount);
    DPRINT("TotalTerminalDescriptorCount %lu NonStreamingTerminalDescriptorCount %lu\n", TotalTerminalDescriptorCount, NonStreamingTerminalDescriptorCount);

    /* allocate pins */
    Pins = AllocFunction(sizeof(KSPIN_DESCRIPTOR_EX) * TotalTerminalDescriptorCount);
    if (!Pins)
    {
        /* no memory*/
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    for (Index = 0; Index < TotalTerminalDescriptorCount; Index++)
    {
        if (Index < (TotalTerminalDescriptorCount - NonStreamingTerminalDescriptorCount))
        {
            /* irp sink pins*/
            TerminalDescriptor = UsbAudioGetStreamingTerminalDescriptorByIndex(DeviceExtension->ConfigurationDescriptor, Index);
            ASSERT(TerminalDescriptor != NULL);

            Pins[Index].Dispatch = &UsbAudioPinDispatch;
            Pins[Index].PinDescriptor.InterfacesCount = 1;
            Pins[Index].PinDescriptor.Interfaces = &StandardPinInterface;
            Pins[Index].PinDescriptor.MediumsCount = 1;
            Pins[Index].PinDescriptor.Mediums = &StandardPinMedium;
            Pins[Index].PinDescriptor.Category = UsbAudioGetPinCategoryFromTerminalDescriptor(TerminalDescriptor);
            UsbAudioGetDataRanges(DeviceExtension->ConfigurationDescriptor, TerminalDescriptor->bTerminalID, (PKSDATARANGE**)&Pins[Index].PinDescriptor.DataRanges, &Pins[Index].PinDescriptor.DataRangesCount);

            if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_OUTPUT_TERMINAL)
            {
                Pins[Index].PinDescriptor.Communication = KSPIN_COMMUNICATION_BOTH;
                Pins[Index].PinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
            }
            else if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_INPUT_TERMINAL)
            {
                Pins[Index].PinDescriptor.Communication = KSPIN_COMMUNICATION_SINK;
                Pins[Index].PinDescriptor.DataFlow = KSPIN_DATAFLOW_IN;
            }

            /* data intersect handler */
            Pins[Index].IntersectHandler = UsbAudioPinDataIntersect;

            /* pin flags */
            Pins[Index].Flags = KSPIN_FLAG_PROCESS_IN_RUN_STATE_ONLY | KSFILTER_FLAG_CRITICAL_PROCESSING;

            /* irp sinks / sources can be instantiated */
            Pins[Index].InstancesPossible = 1;
            Pins[Index].InstancesNecessary = 1;
        }
        else
        {
            /* bridge pins */
            TerminalDescriptor = UsbAudioGetNonStreamingTerminalDescriptorByIndex(DeviceExtension->ConfigurationDescriptor, Index - (TotalTerminalDescriptorCount - NonStreamingTerminalDescriptorCount));
            Pins[Index].PinDescriptor.InterfacesCount = 1;
            Pins[Index].PinDescriptor.Interfaces = &StandardPinInterface;
            Pins[Index].PinDescriptor.MediumsCount = 1;
            Pins[Index].PinDescriptor.Mediums = &StandardPinMedium;
            Pins[Index].PinDescriptor.DataRanges = BridgePinAudioFormats;
            Pins[Index].PinDescriptor.DataRangesCount = 1;
            Pins[Index].PinDescriptor.Communication = KSPIN_COMMUNICATION_BRIDGE;
            Pins[Index].PinDescriptor.Category = UsbAudioGetPinCategoryFromTerminalDescriptor(TerminalDescriptor);

            if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_INPUT_TERMINAL)
            {
                Pins[Index].PinDescriptor.DataFlow = KSPIN_DATAFLOW_IN;
            }
            else if (TerminalDescriptor->bDescriptorSubtype == USB_AUDIO_OUTPUT_TERMINAL)
            {
                Pins[Index].PinDescriptor.DataFlow = KSPIN_DATAFLOW_OUT;
            }
        }

    }

    *PinDescriptors = Pins;
    *PinDescriptorSize = sizeof(KSPIN_DESCRIPTOR_EX);
    *PinDescriptorsCount = TotalTerminalDescriptorCount;

    return STATUS_SUCCESS;
}

NTSTATUS
USBAudioInitComponentId(
    PKSDEVICE Device,
    IN PKSCOMPONENTID ComponentId)
{
    PDEVICE_EXTENSION DeviceExtension;

    /* get device extension */
    DeviceExtension = Device->Context;

    INIT_USBAUDIO_MID(&ComponentId->Manufacturer, DeviceExtension->DeviceDescriptor->idVendor);
    INIT_USBAUDIO_PID(&ComponentId->Product, DeviceExtension->DeviceDescriptor->idProduct);

    //ComponentId->Component = KSCOMPONENTID_USBAUDIO;
    UNIMPLEMENTED
    return STATUS_NOT_IMPLEMENTED;
}


NTSTATUS
NTAPI
USBAudioCreateFilterContext(
    PKSDEVICE Device)
{
    PKSFILTER_DESCRIPTOR FilterDescriptor;
    PKSCOMPONENTID ComponentId;
    NTSTATUS Status;

    /* allocate descriptor */
    FilterDescriptor = AllocFunction(sizeof(KSFILTER_DESCRIPTOR));
    if (!FilterDescriptor)
    {
        /* no memory */
        return USBD_STATUS_INSUFFICIENT_RESOURCES;
    }

    /* init filter descriptor*/
    FilterDescriptor->Version = KSFILTER_DESCRIPTOR_VERSION;
    FilterDescriptor->Flags = 0;
    FilterDescriptor->ReferenceGuid = &KSNAME_Filter;
    FilterDescriptor->Dispatch = &USBAudioFilterDispatch;
    FilterDescriptor->CategoriesCount = 1;
    FilterDescriptor->Categories = &GUID_KSCATEGORY_AUDIO;

    /* init component id*/
    ComponentId = AllocFunction(sizeof(KSCOMPONENTID));
    if (!ComponentId)
    {
        /* no memory */
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    Status = USBAudioInitComponentId(Device, ComponentId);
    if (!NT_SUCCESS(Status))
    {
        /* failed*/
        //FreeFunction(ComponentId);
        //return Status;
    }
    FilterDescriptor->ComponentId = ComponentId;

    /* build pin descriptors */
    Status = USBAudioPinBuildDescriptors(Device, (PKSPIN_DESCRIPTOR_EX *)&FilterDescriptor->PinDescriptors, &FilterDescriptor->PinDescriptorsCount, &FilterDescriptor->PinDescriptorSize);
    if (!NT_SUCCESS(Status))
    {
        /* failed*/
        FreeFunction(ComponentId);
        return Status;
    }

    /* build topology */
    Status = BuildUSBAudioFilterTopology(Device, FilterDescriptor);
    if (!NT_SUCCESS(Status))
    {
        /* failed*/
        //FreeFunction(ComponentId);
        //return Status;
    }

    /* lets create the filter */
    Status = KsCreateFilterFactory(Device->FunctionalDeviceObject, FilterDescriptor, ReferenceString, NULL, KSCREATE_ITEM_FREEONSTOP, NULL, NULL, NULL);
    DPRINT("KsCreateFilterFactory: %x\n", Status);

    return Status;
}

