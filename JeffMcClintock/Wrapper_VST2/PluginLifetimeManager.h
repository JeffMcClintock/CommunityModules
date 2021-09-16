#pragma once
#include <vector>
#include "../se_sdk3/mp_sdk_common.h"
#include "../shared/xplatform.h"
#include "VstFactory.h"
#include "AEffectWrapper.h"

class PluginLifetimeManager
{
    typedef struct sPluginHandle {
        int32_t handle;
        int32_t refCounter;
    }sPluginHandle;
    std::vector< std::pair< intptr_t, AEffectWrapper* > > pluginInstances;
public:
	PluginLifetimeManager();
	~PluginLifetimeManager();

	static PluginLifetimeManager* Instance();
	AEffectWrapper* Create(intptr_t instance, const std::wstring& filename, long shellPluginId);
    AEffectWrapper* Get(intptr_t instance);
	void Destroy(intptr_t instance);
};

