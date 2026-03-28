class DAB_TrackFilter<Class T>
{
    static array<T> FindOfType(array<CinematicTrackBase> tracks)
    {
        array<T> result = {};

        foreach (CinematicTrackBase track : tracks)
        {
            T cast = T.Cast(track);
            if (cast)
                result.Insert(cast);
        }

        return result;
    }
	
	static array<T> FindOfTypeOwnedByEntity(array<CinematicTrackBase> tracks, string entityName)
    {
        array<T> result = {};

        foreach (CinematicTrackBase track : tracks)
        {
            T cast = T.Cast(track);
            if (!cast) continue;

            if (DAB_CinematicsHelper.GetTrackEntityName(track) == entityName)
                result.Insert(cast);
        }

        return result;
    }
}