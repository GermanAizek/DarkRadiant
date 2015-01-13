#pragma once

#include "ientity.h"
#include "ieclass.h"

namespace entity
{

/// Implementation of the EntityCreator interface for Doom 3
class Doom3EntityCreator :
	public EntityCreator
{
public:

    // EntityCreator implementation
	IEntityNodePtr createEntity(const IEntityClassPtr& eclass) override;
	void connectEntities(const scene::INodePtr& source, const scene::INodePtr& target) override;
    ITargetManagerPtr createTargetManager() override;

	// RegisterableModule implementation
	virtual const std::string& getName() const;
	virtual const StringSet& getDependencies() const;
	virtual void initialiseModule(const ApplicationContext& ctx);
	virtual void shutdownModule();
};
typedef std::shared_ptr<Doom3EntityCreator> Doom3EntityCreatorPtr;

} // namespace entity
