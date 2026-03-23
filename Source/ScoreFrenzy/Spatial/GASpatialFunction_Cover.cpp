#include "GASpatialFunction_Cover.h"

UGASpatialFunction_Cover::UGASpatialFunction_Cover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MinCoverDistance = 400.0f;
	MaxCoverDistance = 2000.0f;
	bRequireLOSToPlayer = false;
}
