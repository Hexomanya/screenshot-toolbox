class DAB_BoneOverlayRenderer
{
    protected ref map<string, vector> m_CachedWorldPositions = new map<string, vector>();
    protected ref array<ref Shape> m_ActiveShapes = new array<ref Shape>();
	protected ref DebugTextScreenSpace m_SelectedBoneText;
	
	protected float m_fCurrentSphereSize = 0;
	
    void DrawAllBones(IEntity entity, array<string> boneNames, string hoveredBone, float cameraTargetDistance, bool hideVolumeBones)
    {
		m_fCurrentSphereSize = DAB_VisConfig.ComputeSphereSize(cameraTargetDistance);
		
		ClearShapes();
        m_CachedWorldPositions.Clear();
		
        if(entity == null)
		{
			Print("DrawAllBones: Provided entity is null.", LogLevel.ERROR);
			return;
		}
		
        Animation anim = entity.GetAnimation();
	
		if(anim == null)
		{
			Print("DrawAllBones: Animation is null.", LogLevel.ERROR);
			return;
		}
		
        vector entityWorld[4];
        entity.GetTransform(entityWorld);

        for (int i = 0; i < boneNames.Count(); i++)
        {
			if (hideVolumeBones && boneNames[i].EndsWith("Volume"))
		        continue;
			
            vector pos;
            int color;

            TNodeId boneId = anim.GetBoneIndex(boneNames[i]);

            if (boneId == -1) continue;
            
			
            vector boneLocal[4];
            anim.GetBoneMatrix(boneId, boneLocal);

            vector boneWorld[4];
            Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);

            pos   = boneWorld[3];
            color = DAB_VisConfig.COLOR_DEFAULT;
			
			m_CachedWorldPositions.Set(boneNames[i], pos);

			float sphereRadius = m_fCurrentSphereSize;
			if(boneNames[i] == hoveredBone)
			{
				sphereRadius *= DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;
				color = DAB_VisConfig.COLOR_HOVER;
			}
			
            Shape s = Shape.CreateSphere(
                color,
                DAB_VisConfig.SPHERE_FLAGS,
                pos,
                sphereRadius
            );
            m_ActiveShapes.Insert(s);
        }
    }

    void DrawSelectedBone(IEntity entity, string selectedBoneName, float cameraBoneDistance, WorldEditorAPI api)
    {
		m_fCurrentSphereSize = DAB_VisConfig.ComputeSphereSize(cameraBoneDistance) * DAB_VisConfig.SPHERE_HOVER_MULTIPLIER;
        ClearShapes();

		m_SelectedBoneText = DebugTextScreenSpace.Create(api.GetWorld(), selectedBoneName, DebugTextFlags.FACE_CAMERA, 10, 10);

        Animation anim = entity.GetAnimation();
        if (!anim) return;
		
        TNodeId boneId = anim.GetBoneIndex(selectedBoneName);
        if (boneId == -1) return;

        vector boneLocal[4];
        anim.GetBoneMatrix(boneId, boneLocal);

        vector entityWorld[4];
        entity.GetTransform(entityWorld);

        vector boneWorld[4];
        Math3D.MatrixMultiply4(entityWorld, boneLocal, boneWorld);

        Shape s = Shape.CreateSphere(
            DAB_VisConfig.COLOR_SELECTED,
            ShapeFlags.NOZBUFFER | ShapeFlags.TRANSP,
            boneWorld[3],
            m_fCurrentSphereSize
        );
        m_ActiveShapes.Insert(s);
    }

    string PickBoneAtScreenPos(int mouseX, int mouseY, IEntity entity, WorldEditorAPI api)
    {
        if (m_CachedWorldPositions.IsEmpty()) return "";

        vector traceStart, traceEnd, traceDir;
        api.TraceWorldPos(mouseX, mouseY, TraceFlags.ENTS, traceStart, traceEnd, traceDir);

        float bestT = float.MAX;
        string bestBone = "";

        for (MapIterator it = m_CachedWorldPositions.Begin(); it != m_CachedWorldPositions.End(); it = m_CachedWorldPositions.Next(it))
		{
		    string boneName = m_CachedWorldPositions.GetIteratorKey(it);
		    vector position = m_CachedWorldPositions.GetIteratorElement(it);
		
		    if (position == vector.Zero) continue;
		
		    float t;
		    if (RaySphereIntersect(traceStart, traceDir, position, m_fCurrentSphereSize, t))
		    {
		        if (t < bestT)
		        {
		            bestT = t;
		            bestBone = boneName;
		        }
		    }
		}
        return bestBone;
    }

    void Clear()
    {
        ClearShapes();
        m_CachedWorldPositions.Clear();
    }

    protected void ClearShapes()
	{
	    // Don't manually delete — just release all references.
	    // When ref count hits zero, Enforce Script GC handles cleanup.
	    m_ActiveShapes.Clear();
		m_SelectedBoneText = null;
	}

    protected bool RaySphereIntersect(vector ro, vector rd, vector sc, float r, out float t)
    {
        vector oc = ro - sc;
        float  b  = vector.Dot(oc, rd);
        float  c  = vector.Dot(oc, oc) - r * r;
        float  disc = b * b - c;
        if (disc < 0) return false;
        t = -b - Math.Sqrt(disc);
        return t >= 0;
    }
}