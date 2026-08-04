// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualMesh.h"

#include "Engine/StaticMesh.h"

namespace GPPrimitiveVisualMesh
{
	FString GetEngineShapePath(EGP_PrimitiveShape Shape)
	{
		switch (Shape)
		{
		case EGP_PrimitiveShape::Cube:
			return TEXT("/Engine/BasicShapes/Cube.Cube");
		case EGP_PrimitiveShape::Sphere:
			return TEXT("/Engine/BasicShapes/Sphere.Sphere");
		case EGP_PrimitiveShape::Cone:
			return TEXT("/Engine/BasicShapes/Cone.Cone");
		case EGP_PrimitiveShape::Capsule:
			return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		case EGP_PrimitiveShape::Cylinder:
		default:
			return TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		}
	}

	UStaticMesh* ResolveShapeMesh(EGP_PrimitiveShape Shape)
	{
		const FString Path = GetEngineShapePath(Shape);
		return LoadObject<UStaticMesh>(nullptr, *Path);
	}
}
