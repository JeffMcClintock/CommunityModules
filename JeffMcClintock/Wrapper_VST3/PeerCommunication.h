#pragma once
/*
#include "PeerCommunication.h"
*/
#include "../se_sdk3/mp_sdk_audio.h"

namespace Steinberg
{
	namespace Vst
	{
		class IComponent;
		class IAudioProcessor;
	}
}

struct DECLSPEC_NOVTABLE IVST3PluginOwner : public gmpi::IMpUnknown
{
	virtual int32_t MP_STDCALL registerProcessor(Steinberg::Vst::IComponent** component, Steinberg::Vst::IAudioProcessor** vstEffect_) = 0;
};

// GUID for IVST3ControllerHost
// {78EBE1C5-FB7C-4776-947B-0B3105FE153A}
static const gmpi::MpGuid WV_IID_VST3CONTROLLERHOST =
{ 0x78ebe1c5, 0xfb7c, 0x4776, { 0x94, 0x7b, 0xb, 0x31, 0x5, 0xfe, 0x15, 0x3a } };
