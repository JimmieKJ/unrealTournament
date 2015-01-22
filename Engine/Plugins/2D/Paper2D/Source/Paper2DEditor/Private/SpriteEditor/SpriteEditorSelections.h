// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FSelectionTypes

class FSelectionTypes
{
public:
	static const FName Vertex;
	static const FName Edge;
	static const FName Pivot;
	static const FName Socket;
	static const FName SourceRegion;
private:
	FSelectionTypes() {}
};

class FSpriteSelectedVertex;

//////////////////////////////////////////////////////////////////////////
// FSelectedItem

class FSelectedItem
{
protected:
	FSelectedItem(FName InTypeName)
		: TypeName(InTypeName)
	{
	}

protected:
	FName TypeName;
public:
	virtual bool IsA(FName TestType) const
	{
		return TestType == TypeName;
	}

	virtual uint32 GetTypeHash() const
	{
		return 0;
	}

	virtual bool Equals(const FSelectedItem& OtherItem) const
	{
		return false;
	}

	virtual void ApplyDelta(const FVector2D& Delta)
	{
	}

	//@TODO: Doesn't belong here in base!
	virtual void SplitEdge()
	{
	}

	virtual FVector GetWorldPos() const
	{
		return FVector::ZeroVector;
	}

	virtual const FSpriteSelectedVertex* CastSelectedVertex() const
	{
		return nullptr;
	}

	virtual ~FSelectedItem() {}
};

inline uint32 GetTypeHash(const FSelectedItem& Vertex)
{
	return Vertex.GetTypeHash();
}

inline bool operator==(const FSelectedItem& V1, const FSelectedItem& V2)
{
	return V1.Equals(V2);
}

//////////////////////////////////////////////////////////////////////////
// FSpriteSelectedSourceRegion

class FSpriteSelectedSourceRegion : public FSelectedItem
{
public:
	int32 VertexIndex;
	TWeakObjectPtr<UPaperSprite> SpritePtr;

public:
	FSpriteSelectedSourceRegion()
		: FSelectedItem(FSelectionTypes::SourceRegion)
		, VertexIndex(0)
	{
	}

	virtual uint32 GetTypeHash() const override
	{
		return VertexIndex;
	}

	virtual bool Equals(const FSelectedItem& OtherItem) const override
	{
		if (OtherItem.IsA(FSelectionTypes::SourceRegion))
		{
			const FSpriteSelectedSourceRegion& V1 = *this;
			const FSpriteSelectedSourceRegion& V2 = *(FSpriteSelectedSourceRegion*)(&OtherItem);

			return (V1.VertexIndex == V2.VertexIndex) && (V1.SpritePtr == V2.SpritePtr);
		}
		else
		{
			return false;
		}
	}

	virtual void ApplyDeltaIndexed(const FVector2D& WorldSpaceDelta, int32 TargetVertexIndex)
	{
		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			UTexture2D* SourceTexture = Sprite->GetSourceTexture();
			const FVector2D SourceDims = (SourceTexture != nullptr) ? FVector2D(SourceTexture->GetSurfaceWidth(), SourceTexture->GetSurfaceHeight()) : FVector2D::ZeroVector;
            int XAxis = 0; // -1 = min, 0 = none, 1 = max
            int YAxis = 0; // ditto
            
			switch (VertexIndex)
			{
            case 0: // Top left
                XAxis = -1;
                YAxis = -1;
                break;
            case 1: // Top right
                XAxis = 1;
                YAxis = -1;
                break;
            case 2: // Bottom right
                XAxis = 1;
                YAxis = 1;
                break;
            case 3: // Bottom left
                XAxis = -1;
                YAxis = 1;
                break;
            case 4: // Top
                YAxis = -1;
                break;
            case 5: // Right
                XAxis = 1;
                break;
            case 6: // Bottom
                YAxis = 1;
                break;
            case 7: // Left
                XAxis = -1;
                break;
			default:
				break;
			}

			const FVector2D TextureSpaceDelta = Sprite->ConvertWorldSpaceDeltaToTextureSpace(PaperAxisX * WorldSpaceDelta.X + PaperAxisY * WorldSpaceDelta.Y);

            if (XAxis == -1)
            {
                float AllowedDelta = FMath::Clamp(TextureSpaceDelta.X, -Sprite->SourceUV.X, Sprite->SourceDimension.X - 1);
                Sprite->SourceUV.X += AllowedDelta;
                Sprite->SourceDimension.X -= AllowedDelta;
            }
            else if (XAxis == 1)
            {
				Sprite->SourceDimension.X = FMath::Clamp(Sprite->SourceDimension.X + TextureSpaceDelta.X, 1.0f, SourceDims.X - Sprite->SourceUV.X);
            }
            
            if (YAxis == -1)
            {
				float AllowedDelta = FMath::Clamp(TextureSpaceDelta.Y, -Sprite->SourceUV.Y, Sprite->SourceDimension.Y - 1);
				Sprite->SourceUV.Y += AllowedDelta;
				Sprite->SourceDimension.Y -= AllowedDelta;
            }
            else if (YAxis == 1)
            {
				Sprite->SourceDimension.Y = FMath::Clamp(Sprite->SourceDimension.Y + TextureSpaceDelta.Y, 1.0f, SourceDims.Y - Sprite->SourceUV.Y);
            }
		}
	}

	FVector GetWorldPosIndexed(int32 TargetVertexIndex) const
	{
		FVector Result = FVector::ZeroVector;

		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			UTexture2D* SourceTexture = Sprite->GetSourceTexture();
			const FVector2D SourceDims = (SourceTexture != nullptr) ? FVector2D(SourceTexture->GetSurfaceWidth(), SourceTexture->GetSurfaceHeight()) : FVector2D::ZeroVector;
			FVector2D BoundsVertex = Sprite->SourceUV;
			switch (VertexIndex)
			{
            case 0:
                break;
            case 1:
                BoundsVertex.X += Sprite->SourceDimension.X;
                break;
            case 2:
                BoundsVertex.X += Sprite->SourceDimension.X;
                BoundsVertex.Y += Sprite->SourceDimension.Y;
                break;
            case 3:
                BoundsVertex.Y += Sprite->SourceDimension.Y;
                break;
			case 4:
				BoundsVertex.X += Sprite->SourceDimension.X * 0.5f;
				break;
			case 5:
				BoundsVertex.X += Sprite->SourceDimension.X;
				BoundsVertex.Y += Sprite->SourceDimension.Y * 0.5f;
				break;
			case 6:
				BoundsVertex.X += Sprite->SourceDimension.X * 0.5f;
				BoundsVertex.Y += Sprite->SourceDimension.Y;
				break;
			case 7:
				BoundsVertex.Y += Sprite->SourceDimension.Y * 0.5f;
				break;
			default:
				break;
			}

			const FVector PixelSpaceResult = BoundsVertex.X * PaperAxisX + (SourceDims.Y - BoundsVertex.Y) * PaperAxisY;
			Result = PixelSpaceResult * Sprite->GetUnrealUnitsPerPixel();
		}

		return Result;
	}

	virtual void ApplyDelta(const FVector2D& Delta) override
	{
		ApplyDeltaIndexed(Delta, VertexIndex);
	}

	FVector GetWorldPos() const override
	{
		return GetWorldPosIndexed(VertexIndex);
	}

	virtual void SplitEdge() override
	{
		// Nonsense operation on a vertex, do nothing
	}
};


//////////////////////////////////////////////////////////////////////////
// FSpriteSelectedVertex

class FSpriteSelectedVertex : public FSelectedItem
{
public:
	int32 VertexIndex;
	int32 PolygonIndex;

	// If true, it's render data, otherwise it's collision data
	bool bRenderData;

	TWeakObjectPtr<UPaperSprite> SpritePtr;
public:
	FSpriteSelectedVertex()
		: FSelectedItem(FSelectionTypes::Vertex)
		, VertexIndex(0)
		, PolygonIndex(0)
		, bRenderData(false)
	{
	}

	virtual bool IsValidInEditor(UPaperSprite* Sprite, bool bRenderData) const
	{
		if (SpritePtr == Sprite && this->bRenderData == bRenderData)
		{
			if (UPaperSprite* Sprite = SpritePtr.Get())
			{
				FSpritePolygonCollection& Geometry = bRenderData ? Sprite->RenderGeometry : Sprite->CollisionGeometry;
				if (Geometry.Polygons.IsValidIndex(PolygonIndex) && Geometry.Polygons[PolygonIndex].Vertices.IsValidIndex(VertexIndex))
				{
					return true;
				}
			}
		}
		return false;
	}

	virtual const FSpriteSelectedVertex* CastSelectedVertex() const
	{
		return (FSpriteSelectedVertex*)this;
	}

	virtual uint32 GetTypeHash() const override
	{
		return VertexIndex + (PolygonIndex * 311) + (bRenderData ? 1063 : 0);
	}

	virtual bool Equals(const FSelectedItem& OtherItem) const override
	{
		if (OtherItem.IsA(FSelectionTypes::Vertex))
		{
			const FSpriteSelectedVertex& V1 = *this;
			const FSpriteSelectedVertex& V2 = *(FSpriteSelectedVertex*)(&OtherItem);

			return (V1.VertexIndex == V2.VertexIndex) && (V1.PolygonIndex == V2.PolygonIndex) && (V1.bRenderData == V2.bRenderData) && (V1.SpritePtr == V2.SpritePtr);
		}
		else
		{
			return false;
		}
	}

	virtual void ApplyDeltaIndexed(const FVector2D& Delta, int32 TargetVertexIndex)
	{
		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			FSpritePolygonCollection& Geometry = bRenderData ? Sprite->RenderGeometry : Sprite->CollisionGeometry;
			if (Geometry.Polygons.IsValidIndex(PolygonIndex))
			{
				TArray<FVector2D>& Vertices = Geometry.Polygons[PolygonIndex].Vertices;
				TargetVertexIndex = (Vertices.Num() > 0) ? (TargetVertexIndex % Vertices.Num()) : 0;
				if (Vertices.IsValidIndex(TargetVertexIndex))
				{
					const FVector WorldSpaceDelta = PaperAxisX * Delta.X + PaperAxisY * Delta.Y;
					const FVector2D TextureSpaceDelta = Sprite->ConvertWorldSpaceDeltaToTextureSpace(WorldSpaceDelta);
					Vertices[TargetVertexIndex] += TextureSpaceDelta;

					Geometry.GeometryType = ESpritePolygonMode::FullyCustom;
				}
			}
		}
	}

	FVector GetWorldPosIndexed(int32 TargetVertexIndex) const
	{
		FVector Result = FVector::ZeroVector;

		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			FSpritePolygonCollection& Geometry = bRenderData ? Sprite->RenderGeometry : Sprite->CollisionGeometry;

			if (Geometry.Polygons.IsValidIndex(PolygonIndex))
			{
				const TArray<FVector2D>& Vertices = Geometry.Polygons[PolygonIndex].Vertices;
				TargetVertexIndex = (Vertices.Num() > 0) ? (TargetVertexIndex % Vertices.Num()) : 0;
				if (Vertices.IsValidIndex(TargetVertexIndex))
				{
					Result = Sprite->ConvertTextureSpaceToWorldSpace(Vertices[TargetVertexIndex]);
				}
			}
		}

		return Result;
	}

	virtual void ApplyDelta(const FVector2D& Delta) override
	{
		ApplyDeltaIndexed(Delta, VertexIndex);
	}

	FVector GetWorldPos() const override
	{
		return GetWorldPosIndexed(VertexIndex);
	}

	virtual void SplitEdge() override
	{
		// Nonsense operation on a vertex, do nothing
	}
};


//////////////////////////////////////////////////////////////////////////
// FSpriteSelectedEdge
//@TODO: Much hackery lies here; need a real data structure!


class FSpriteSelectedEdge : public FSpriteSelectedVertex
{
	// Note: Defined based on a vertex index; this is the edge between the vertex and the next one.
public:
	FSpriteSelectedEdge()
	{
		TypeName = FSelectionTypes::Edge;
	}

	virtual bool Equals(const FSelectedItem& OtherItem) const override
	{
		if (OtherItem.IsA(FSelectionTypes::Edge))
		{
			const FSpriteSelectedEdge& E1 = *this;
			const FSpriteSelectedEdge& E2 = *(FSpriteSelectedEdge*)(&OtherItem);

			return (E1.VertexIndex == E2.VertexIndex) && (E1.PolygonIndex == E2.PolygonIndex) && (E1.bRenderData == E2.bRenderData) && (E1.SpritePtr == E2.SpritePtr);
		}
		else
		{
			return false;
		}
	}

	virtual void ApplyDelta(const FVector2D& Delta) override
	{
		ApplyDeltaIndexed(Delta, VertexIndex);
		ApplyDeltaIndexed(Delta, VertexIndex+1);
	}

	FVector GetWorldPos() const override
	{
		const FVector Pos1 = GetWorldPosIndexed(VertexIndex);
		const FVector Pos2 = GetWorldPosIndexed(VertexIndex+1);

		return (Pos1 + Pos2) * 0.5f;
	}

	virtual void SplitEdge() override
	{
		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			FSpritePolygonCollection& Geometry = bRenderData ? Sprite->RenderGeometry : Sprite->CollisionGeometry;
			Geometry.GeometryType = ESpritePolygonMode::FullyCustom;

			if (Geometry.Polygons.IsValidIndex(PolygonIndex))
			{
				TArray<FVector2D>& Vertices = Geometry.Polygons[PolygonIndex].Vertices;
				if (Vertices.IsValidIndex(VertexIndex))
				{
					const int32 NextVertexIndex = (VertexIndex + 1) % Vertices.Num();

					const FVector2D NewPos = (Vertices[VertexIndex] + Vertices[NextVertexIndex]) * 0.5f;
					
					Vertices.Insert(NewPos, VertexIndex+1);
				}
			}
		}
	}
};



//////////////////////////////////////////////////////////////////////////
// FSpriteSelectedSocket

class FSpriteSelectedSocket : public FSelectedItem
{
public:
	FName SocketName;
	TWeakObjectPtr<UPaperSprite> SpritePtr;

public:
	FSpriteSelectedSocket()
		: FSelectedItem(FSelectionTypes::Socket)
	{
	}

	virtual bool Equals(const FSelectedItem& OtherItem) const override
	{
		if (OtherItem.IsA(FSelectionTypes::Socket))
		{
			const FSpriteSelectedSocket& S1 = *this;
			const FSpriteSelectedSocket& S2 = *(FSpriteSelectedSocket*)(&OtherItem);

			return (S1.SocketName == S2.SocketName) && (S1.SpritePtr == S2.SpritePtr);
		}
		else
		{
			return false;
		}
	}

	//@TODO: Currently sockets are in unflipped pivot space,
	virtual void ApplyDelta(const FVector2D& Delta) override
	{
		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			if (FPaperSpriteSocket* Socket = Sprite->FindSocket(SocketName))
			{
				const FVector Delta3D_UU = (PaperAxisX * Delta.X) + (PaperAxisY * -Delta.Y);
				const FVector Delta3D = Delta3D_UU * Sprite->GetPixelsPerUnrealUnit();
				Socket->LocalTransform.SetLocation(Socket->LocalTransform.GetLocation() + Delta3D);
			}
		}
	}

	FVector GetWorldPos() const override
	{
		if (UPaperSprite* Sprite = SpritePtr.Get())
		{
			if (FPaperSpriteSocket* Socket = Sprite->FindSocket(SocketName))
			{
 				const FVector PivotSpacePos = Socket->LocalTransform.GetLocation();
				return Sprite->GetPivotToWorld().TransformPosition(PivotSpacePos);
			}
		}

		return FVector::ZeroVector;
	}

	virtual void SplitEdge() override
	{
		// Nonsense operation on a socket, do nothing
	}
};
