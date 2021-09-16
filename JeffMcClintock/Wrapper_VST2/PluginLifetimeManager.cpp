#include "PluginLifetimeManager.h"

PluginLifetimeManager::PluginLifetimeManager()
{
}

PluginLifetimeManager::~PluginLifetimeManager()
{
    while(!pluginInstances.empty())
    {
		Destroy(pluginInstances.begin()->first);
    }
}

PluginLifetimeManager* PluginLifetimeManager::Instance()
{
	static PluginLifetimeManager singleton;
	return &singleton;
}

AEffectWrapper* PluginLifetimeManager::Create(intptr_t instance, const std::wstring& filename, long shellPluginId)
{
    AEffectWrapper* pAEffectWrapper = 0;
    //check whether AEffectWrapper already exists - if it does no need to load another child plugin
    for (auto& p : pluginInstances)
    {
        if (p.first == instance)
        {
            pAEffectWrapper = p.second;
			break;
        }
    }
    
    if (!pAEffectWrapper)
    {
        pAEffectWrapper = new AEffectWrapper();
        pAEffectWrapper->LoadDll(filename, shellPluginId);

        if( pAEffectWrapper->IsLoaded() )
        {
            pAEffectWrapper->dispatcher(effOpen);
            pluginInstances.push_back({ instance, pAEffectWrapper });
        }
        else
        {
            delete pAEffectWrapper;
            pAEffectWrapper = 0;
        }
	}
    
    return pAEffectWrapper;
}

AEffectWrapper* PluginLifetimeManager::Get(intptr_t instance)
{
    for( auto p : pluginInstances )
    {
        if( p.first == instance)
            return p.second;
    }
    
    return 0;
}

void PluginLifetimeManager::Destroy(intptr_t instance)
{
    for( auto it = pluginInstances.begin(); it != pluginInstances.end(); ++it )
	{
		if( ( *it ).first == instance)
		{
            delete ( *it ).second;
            pluginInstances.erase(it);
            break;
		}
	}
}