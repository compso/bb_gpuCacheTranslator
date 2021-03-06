/* (c)2012 BlueBolt Ltd. All rights reserved.
 *
 * GpuCacheTranslator.cpp
 *
 *  Created on: 20 Jul 2012
 *      Author: ashley-r
 */

#include <maya/MFnDagNode.h>
#include <maya/MBoundingBox.h>
#include <maya/MPlugArray.h>
#include <maya/MTypes.h>

#include "gpuCacheTranslator.h"

/*
 * Return a new string with all occurrences of 'from' replaced with 'to'
 */
std::string replace_all(const MString &str, const char *from, const char *to)
{
    std::string result(str.asChar());
    std::string::size_type
        index = 0,
        from_len = strlen(from),
        to_len = strlen(to);
    while ((index = result.find(from, index)) != std::string::npos) {
        result.replace(index, from_len, to);
        index += to_len;
    }
    return result;
}


union DJB2HashUnion{
    unsigned int hash;
    int hashInt;
};

int DJB2Hash(unsigned char *str)
{
    DJB2HashUnion hashUnion;
    hashUnion.hash = 5381;
    int c;

    while ((c = *str++))
        hashUnion.hash = ((hashUnion.hash << 5) + hashUnion.hash) + c; /* hash * 33 + c */

    return hashUnion.hashInt;
}


AtNode* GpuCacheTranslator::CreateArnoldNodes()
{
    AiMsgDebug("[GpuCacheTranslator] CreateArnoldNodes()");
    m_isMasterDag =  IsMasterInstance();
    m_masterDag = GetMasterInstance();
    if (m_isMasterDag)
    {
      return AddArnoldNode( "alembic_loader" );
    }
    else
    {
      return AddArnoldNode( "ginstance" );
    }
}

void GpuCacheTranslator::Delete()
{
   // If the procedural has been expanded at export,
   // we need to delete all the created nodes here
   AiMsgDebug("[GpuCacheTranslator] Delete()");
   CShapeTranslator::Delete();
}

void GpuCacheTranslator::RequestUpdate()
{
    SetUpdateMode(AI_RECREATE_NODE);
    CShapeTranslator::RequestUpdate();
}

void GpuCacheTranslator::Export( AtNode* instance )
{
    if (IsExported())
    {
        // Since we always use AI_RECREATE_NODE during IPR (see RequestUpdate)
        // we should never get here. Early out for safety
        return;
    }
    AiMsgDebug("[GpuCacheTranslator] Export()");

    const char* nodeType = AiNodeEntryGetName(AiNodeGetNodeEntry(instance));
    if (strcmp(nodeType, "ginstance") == 0)
    {
        ExportInstance(instance, m_masterDag, false);
    }
    else
    {

        ExportProcedural(instance, false);
    }
}

AtNode* GpuCacheTranslator::ExportInstance(AtNode *instance, const MDagPath& masterInstance, bool update)
{
   AtNode* masterNode = AiNodeLookUpByName(masterInstance.partialPathName().asChar());


   int instanceNum = m_dagPath.instanceNumber();

   if ( instanceNum > 0 )
     {
       std::cout << "ExportInstance::instanceNum :: " << instanceNum << std::endl;

       AiNodeSetStr(instance, "name", m_dagPath.partialPathName().asChar());

       ExportMatrix(instance);

       AiNodeSetPtr(instance, "node", masterNode);
       AiNodeSetBool(instance, "inherit_xform", false);
       int visibility = AiNodeGetInt(masterNode, "visibility");
       AiNodeSetInt(instance, "visibility", visibility);

       AiNodeSetPtr( instance, "shader", arnoldShader(instance) );

       // Export light linking per instance
       ExportLightLinking(instance);
     }
   return instance;
}

void GpuCacheTranslator::ExportProcedural( AtNode *node, bool update)
{
        AiMsgDebug("[GpuCacheTranslator] ExportProcedural()");
        // do basic node export
        ExportMatrix( node );

        // AiNodeSetPtr( node, "shader", arnoldShader(node) );

        AiNodeSetInt( node, "visibility", ComputeVisibility() );

        MPlug plug = FindMayaPlug( "receiveShadows" );
        if( !plug.isNull() )
        {
                AiNodeSetBool( node, "receive_shadows", plug.asBool() );
        }

        plug = FindMayaPlug( "aiSelfShadows" );
        if( !plug.isNull() )
        {
                AiNodeSetBool( node, "self_shadows", plug.asBool() );
        }

        plug = FindMayaPlug( "aiOpaque" );
        if( !plug.isNull() )
        {
                AiNodeSetBool( node, "opaque", plug.asBool() );
        }

        MStatus status;
        MFnDependencyNode dnode(m_dagPath.node(), &status);
        if (status)
            AiNodeSetInt(node, "id", DJB2Hash((unsigned char*)dnode.name().asChar()));

        // now set the procedural-specific parameters

        if (!update){                            

            MFnDagNode fnDagNode( m_dagPath );
            MBoundingBox bound = fnDagNode.boundingBox();

            AiNodeSetVec( node, "min", bound.min().x-m_dispPadding, bound.min().y-m_dispPadding, bound.min().z-m_dispPadding );
            AiNodeSetVec( node, "max", bound.max().x+m_dispPadding, bound.max().y, bound.max().z+m_dispPadding );

            // const char *dsoPath = getenv( "ALEMBIC_ARNOLD_PROCEDURAL_PATH" );
            // AiNodeSetStr( node, "filename",  dsoPath ? dsoPath : "bb_AlembicArnoldProcedural.so" );

            // Set the parameters for the procedural

            //abcFile path
            MString abcFile = fnDagNode.findPlug("cacheFileName").asString().expandEnvironmentVariablesAndTilde();

            //object path
            MString objectPath = fnDagNode.findPlug("cacheGeomPath").asString();

            //object pattern
            MString objectPattern = "*";

            plug = FindMayaPlug( "objectPattern" );
            if (!plug.isNull() )
            {
                  if (plug.asString() != "")
                  {
                    objectPattern = plug.asString();
                  }
            }

            //object pattern
            MString excludePattern = "";

            plug = FindMayaPlug( "excludePattern" );
            if (!plug.isNull() )
            {
                  if (plug.asString() != "")
                  {
                    excludePattern = plug.asString();
                  }
            }

            float shutterOpen = 0.0;
            plug = FindMayaPlug( "shutterOpen" );
            if (!plug.isNull() )
            {
                    shutterOpen = plug.asFloat();
            }

            float shutterClose = 0.0;
            plug = FindMayaPlug( "shutterClose" );
            if (!plug.isNull() )
            {
                    shutterClose = plug.asFloat();
            }

            float timeOffset = 0.0;
            plug = FindMayaPlug( "timeOffset" );
            if (!plug.isNull() )
            {
                    timeOffset = plug.asFloat();
            }

            float frame = 0.0;
            plug = FindMayaPlug( "frame" );
            if (!plug.isNull() )
            {
                    frame = plug.asFloat();
            }

            int subDIterations = 0;
            plug = FindMayaPlug( "ai_subDIterations" );
            if (!plug.isNull() )
            {
                    subDIterations = plug.asInt();
            }

            MString nameprefix = "";
            plug = FindMayaPlug( "namePrefix" );
            if (!plug.isNull() )
            {
                    nameprefix = plug.asString();
            }

            // bool exportFaceIds = fnDagNode.findPlug("exportFaceIds").asBool();

            bool makeInstance = false; 
            plug = FindMayaPlug( "makeInstance" );
            if (!plug.isNull() )
            {
                    makeInstance = plug.asBool();
            }

            // bool loadAtInit = true; 
            // plug = FindMayaPlug( "loadAtInit" );
            // if (!plug.isNull() )
            // {
            //         loadAtInit = plug.asBool();
            // }
            
            bool flipv = false; 
            plug = FindMayaPlug( "flipv" );
            if (!plug.isNull() )
            {
                    flipv = plug.asBool();
            }

            bool invertNormals = false; 
            plug = FindMayaPlug( "invertNormals" );
            if (!plug.isNull() )
            {
                    invertNormals = plug.asBool();
            }
            
            short i_subDUVSmoothing = 1;
            plug = FindMayaPlug( "ai_subDUVSmoothing" );
            if (!plug.isNull() )
            {
                    i_subDUVSmoothing = plug.asShort();
            }

            MString  subDUVSmoothing;

            switch (i_subDUVSmoothing)
            {
              case 0:
                subDUVSmoothing = "pin_corners";
                break;
              case 1:
                subDUVSmoothing = "pin_borders";
                break;
              case 2:
                subDUVSmoothing = "linear";
                break;
              case 3:
                subDUVSmoothing = "smooth";
                break;
              default :
                subDUVSmoothing = "pin_corners";
                break;
            }

            // MTime curTime = MAnimControl::currentTime();
            // fnDagNode.findPlug("time").getValue( frame );

            // MTime frameOffset;
            // fnDagNode.findPlug("timeOffset").getValue( frameOffset );

            // float time = curTime.as(MTime::kFilm)+timeOffset;
            float time = frame+timeOffset;

            MString argsString;
            if (objectPath != "|"){
                    argsString += " -objectpath ";
                    // convert "|" to "/"

                    argsString += MString(replace_all(objectPath,"|","/").c_str());
            }
            if (objectPattern != "*"){
                    argsString += " -pattern ";
                    argsString += objectPattern;
            }
            if (excludePattern != ""){
                    argsString += " -excludepattern ";
                    argsString += excludePattern;
            }
            if (shutterOpen != 0.0){
                    argsString += " -shutteropen ";
                    argsString += shutterOpen;
            }
            if (shutterClose != 0.0){
                    argsString += " -shutterclose ";
                    argsString += shutterClose;
            }
            if (subDIterations != 0){
                    argsString += " -subditerations ";
                    argsString += subDIterations;
                    argsString += " -subduvsmoothing ";
                    argsString += subDUVSmoothing;
            }
            if (makeInstance){
                    argsString += " -makeinstance ";
            }
            if (nameprefix != ""){
                    argsString += " -nameprefix ";
                    argsString += nameprefix;
            }
            if (flipv){
                    argsString += " -flipv ";
            }
            if (invertNormals){
                    argsString += " -invertNormals ";
            }
            argsString += " -filename ";
            argsString += abcFile;
            argsString += " -frame ";
            argsString += time;

            if (m_displaced){

                argsString += " -disp_map ";
                argsString += AiNodeGetName(m_dispNode);

            }

            AiNodeSetStr(node, "data", argsString.asChar());
            // AiNodeSetBool( node, "load_at_init", loadAtInit ); 

            ExportUserAttrs(node);

            // export curve attributes
            ExportCurveAttrs(node);

            // Export light linking per instance
            ExportLightLinking(node);

        } 
}

void GpuCacheTranslator::ExportUserAttrs( AtNode *node )
{
        // Get the optional attributes and export them as user vars

        MPlug plug = FindMayaPlug( "shaderAssignation" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "shaderAssignation", "constant STRING" );
                AiNodeSetStr( node, "shaderAssignation", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "displacementAssignation" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "displacementAssignation", "constant STRING" );
                AiNodeSetStr( node, "displacementAssignation", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "shaderAssignmentfile" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "shaderAssignmentfile", "constant STRING" );
                AiNodeSetStr( node, "shaderAssignmentfile", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "overrides" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "overrides", "constant STRING" );
                AiNodeSetStr( node, "overrides", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "overridefile" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "overridefile", "constant STRING" );
                AiNodeSetStr( node, "overridefile", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "userAttributes" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "userAttributes", "constant STRING" );
                AiNodeSetStr( node, "userAttributes", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "userAttributesfile" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "userAttributesfile", "constant STRING" );
                AiNodeSetStr( node, "userAttributesfile", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "skipJson" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "skipJson", "constant BOOL" );   
                AiNodeSetBool( node, "skipJson", plug.asBool() );
        }

        plug = FindMayaPlug( "skipShaders" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "skipShaders", "constant BOOL" );   
                AiNodeSetBool( node, "skipShaders", plug.asBool() );
        }

        plug = FindMayaPlug( "skipOverrides" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "skipOverrides", "constant BOOL" );   
                AiNodeSetBool( node, "skipOverrides", plug.asBool() );
        }

        plug = FindMayaPlug( "skipUserAttributes" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "skipUserAttributes", "constant BOOL" );   
                AiNodeSetBool( node, "skipUserAttributes", plug.asBool() );
        }

        plug = FindMayaPlug( "skipDisplacements" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "skipDisplacements", "constant BOOL" );                          
                AiNodeSetBool( node, "skipDisplacements", plug.asBool() );
        }

        plug = FindMayaPlug( "objectPattern" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "objectPattern", "constant STRING" );
                AiNodeSetStr( node, "objectPattern", plug.asString().asChar() );
        }                       
        
        plug = FindMayaPlug( "assShaders" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "assShaders", "constant STRING" );
                AiNodeSetStr( node, "assShaders", plug.asString().asChar() );
        }

        plug = FindMayaPlug( "radiusPoint" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "radiusPoint", "constant FLOAT" );
                AiNodeSetFlt( node, "radiusPoint", plug.asFloat() );
        }


        plug = FindMayaPlug( "scaleVelocity" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "scaleVelocity", "constant FLOAT" );
                AiNodeSetFlt( node, "scaleVelocity", plug.asFloat() );
        }

}

void GpuCacheTranslator::ExportCurveAttrs( AtNode *node )
{
        MPlug plug = FindMayaPlug( "radiusCurve" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "radiusCurve", "constant FLOAT" );
                AiNodeSetFlt( node, "radiusCurve", plug.asFloat() );
        }

        plug = FindMayaPlug( "modeCurve" );
        if( !plug.isNull() )
        {
                AiNodeDeclare( node, "modeCurve", "constant STRING" );

                int modeCurveInt = plug.asInt();

                if (modeCurveInt == 1)
                   AiNodeSetStr(node, "modeCurve", "thick");
                else if (modeCurveInt == 2)
                   AiNodeSetStr(node, "modeCurve", "oriented");
                else
                   AiNodeSetStr(node, "modeCurve", "ribbon");                               
        }
}

bool GpuCacheTranslator::RequiresMotionData()
{
        return IsMotionBlurEnabled( MTOA_MBLUR_OBJECT ) && IsLocalMotionBlurEnabled();
}

void GpuCacheTranslator::ExportMotion( AtNode *node )
{
        if( !IsMotionBlurEnabled() )
        {
                return;
        }

        ExportMatrix( node );
}

void GpuCacheTranslator::nodeInitialiser( CAbTranslator context )
{
        CExtensionAttrHelper helper( context.maya, "procedural" );
        CShapeTranslator::MakeCommonAttributes(helper);
        CShapeTranslator::MakeMayaVisibilityFlags(helper);

        CAttrData data;
        // make the attributes 
        data.defaultValue.STR() = AtString("");
        data.name = "aiTraceSets";
        data.shortName = "trace_sets";
        helper.MakeInputString ( data );

        data.defaultValue.STR() = AtString("");
        data.name = "aiSssSetname";
        data.shortName = "ai_sss_setname";
        helper.MakeInputString ( data );

        data.defaultValue.STR() = AtString("");
        data.name = "objectPattern";
        data.shortName = "object_pattern";
        helper.MakeInputString ( data );

        data.defaultValue.STR() = AtString("");
        data.name = "excludePattern";
        data.shortName = "exclude_pattern";
        helper.MakeInputString ( data );

        data.defaultValue.STR() = AtString("");
        data.name = "namePrefix";
        data.shortName = "name_prefix";
        helper.MakeInputString ( data );                        

        data.defaultValue.BOOL() = false;
        data.name = "makeInstance";
        data.shortName = "make_instance";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "flipv";
        data.shortName = "flip_v";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "invertNormals";
        data.shortName = "invert_normals";
        helper.MakeInputBoolean(data);

        data.defaultValue.STR() = AtString("");
        data.name = "shaderAssignation";
        data.shortName = "shader_assignation";
        helper.MakeInputString ( data );    

        data.defaultValue.STR() = AtString("");
        data.name = "displacementAssignation";
        data.shortName = "displacement_assignation";
        helper.MakeInputString ( data );  

        data.defaultValue.STR() = AtString("");
        data.name = "shaderAssignmentfile";
        data.shortName = "shader_assignment_file";
        helper.MakeInputString ( data ); 

        data.defaultValue.STR() = AtString("");
        data.name = "overrides";
        data.shortName = "overrides";
        helper.MakeInputString ( data );                             

        data.defaultValue.STR() = AtString("");
        data.name = "overridefile";
        data.shortName = "override_file";
        helper.MakeInputString ( data );     

        data.defaultValue.STR() = AtString("");
        data.name = "userAttributes";
        data.shortName = "user_attributes";
        helper.MakeInputString ( data );     
        
        data.defaultValue.STR() = AtString("");
        data.name = "userAttributesfile";
        data.shortName = "user_attributes_file";
        helper.MakeInputString ( data );

        data.defaultValue.BOOL() = false;
        data.name = "skipJson";
        data.shortName = "skip_json";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "skipShaders";
        data.shortName = "skip_shaders";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "skipOverrides";
        data.shortName = "skip_overrides";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "skipUserAttributes";
        data.shortName = "skip_user_attributes";
        helper.MakeInputBoolean(data);

        data.defaultValue.BOOL() = false;
        data.name = "skipDisplacements";
        data.shortName = "skip_displacements";
        helper.MakeInputBoolean(data);                        

        data.defaultValue.STR() = AtString("");
        data.name = "objectPattern";
        data.shortName = "object_pattern";
        helper.MakeInputString ( data );

        data.defaultValue.STR() = AtString("");
        data.name = "assShaders";
        data.shortName = "ass_shaders";
        helper.MakeInputString ( data );   

        data.defaultValue.FLT() = 0.1f;
        data.name = "radiusPoint";
        data.shortName = "radius_point";
        helper.MakeInputFloat(data);     

        data.defaultValue.FLT() = 0.0f;
        data.name = "timeOffset";
        data.shortName = "time_offset";
        helper.MakeInputFloat(data);

        data.defaultValue.FLT() = 0.0f;
        data.name = "frame";
        data.shortName = "frame";
        helper.MakeInputFloat(data);  
        
        // radiusCurve, this can be textured to give varying width along the curve
        data.defaultValue.FLT() = 0.01f;
        data.name = "radiusCurve";
        data.shortName = "radius_curve";
        data.hasMin = true;
        data.min.FLT() = 0.0f;
        data.hasSoftMax = true;
        data.softMax.FLT() = 1.0f;
        helper.MakeInputFloat(data); 

        MStringArray  curveTypeEnum;
        curveTypeEnum.append ( "ribbon" );
        curveTypeEnum.append ( "thick" );
        data.defaultValue.INT() = 0;
        data.name = "modeCurve";
        data.shortName = "mode_curve";
        data.enums = curveTypeEnum;
        helper.MakeInputEnum(data);
        
        data.defaultValue.FLT() = 1.0f;
        data.name = "scaleVelocity";
        data.shortName = "scale_velocity";
        helper.MakeInputFloat(data);  

        data.defaultValue.BOOL() = true;
        data.name = "loadAtInit";
        data.shortName = "load_at_init";
        helper.MakeInputBoolean(data);      
}


void GpuCacheTranslator::GetDisplacement( MObject& obj,
                                          float& dispPadding,
                                          bool& enableAutoBump)
{
   MFnDependencyNode dNode(obj);
   MPlug plug = dNode.findPlug("aiDisplacementPadding");
   if (!plug.isNull())
      dispPadding = AiMax(dispPadding, plug.asFloat());
   if (!enableAutoBump)
   {
      plug = dNode.findPlug("aiDisplacementAutoBump");
      if (!plug.isNull())
         enableAutoBump = enableAutoBump || plug.asBool();
   }
}

AtNode* GpuCacheTranslator::arnoldShader(AtNode* node)
{
  m_displaced = false;

  float maximumDisplacementPadding = -AI_BIG;
  bool enableAutoBump = false;

  unsigned instNumber = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;
  MPlug shadingGroupPlug = GetNodeShadingGroup(m_dagPath.node(), instNumber);

  //find and export any displacment shaders attached
  // DISPLACEMENT MATERIAL EXPORT
  MPlugArray        connections;
  MFnDependencyNode fnDGShadingGroup(shadingGroupPlug.node());
  MPlug shaderPlug = fnDGShadingGroup.findPlug("displacementShader");
  shaderPlug.connectedTo(connections, true, false);

  // are there any connections to displacementShader?
  if (connections.length() > 0)
  {
     m_displaced = true;
     MObject dispNode = connections[0].node();
     GetDisplacement(dispNode, maximumDisplacementPadding, enableAutoBump);
     m_dispPadding = maximumDisplacementPadding;
     AtNode* dispImage(ExportConnectedNode(connections[0]));

     m_dispNode = dispImage;
  }

  // Only export displacement attributes if a displacement is applied
  if (m_displaced)
  {
      std::cout << "arnoldShader::m_displaced :: " << m_displaced << std::endl;
     // Note that disp_height has no actual influence on the scale of the displacement if it is vector based
     // it only influences the computation of the displacement bounds
    // AiNodeSetFlt(node, "disp_padding", maximumDisplacementPadding);
  }

  // return the exported surface shader
  return ExportConnectedNode( shadingGroupPlug );
}
