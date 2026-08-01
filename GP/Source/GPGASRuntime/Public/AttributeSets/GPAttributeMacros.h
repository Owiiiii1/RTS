// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"

/**
 * Standard GAS attribute accessors (property getter, value get/set/init).
 * Use with FGameplayAttributeData members on UGP_*AttributeSet classes.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
