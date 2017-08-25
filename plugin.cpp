
#include "gpuCacheTranslator.h"

#include <extension/Extension.h>
#include <maya/MTypes.h> 
#include <maya/MDGMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>

extern "C"
{

    DLLEXPORT void initializeExtension( CExtension& extension )
    {
        MStatus status;

        const char * pluginVersion = "2.0.0";

        MString info = "MTOA gpuCache Translator v";
        info += pluginVersion;
        info += " using ";
        info += Alembic::AbcCoreAbstract::GetLibraryVersion().c_str();
        MGlobal::displayInfo(info);

        extension.Requires( "gpuCache" );
        status = extension.RegisterTranslator( "gpuCache",
                                               "",
                                               GpuCacheTranslator::creator,
                                               GpuCacheTranslator::nodeInitialiser);
    }

    DLLEXPORT void deinitializeExtension( CExtension& extension )
    {
    }

} // extern "C"


