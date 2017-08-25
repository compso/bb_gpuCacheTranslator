/* (c)2012 BlueBolt Ltd. All rights reserved.
 *
 * GpuCacheTranslator.h
 *
 *  Created on: 20 Jul 2012
 *      Author: ashley-r
 */

#pragma once

#include <Alembic/AbcCoreAbstract/Foundation.h>
#include "translators/shape/ShapeTranslator.h"

class GpuCacheTranslator : public CShapeTranslator
{
public :

        AtNode* CreateArnoldNodes();

        virtual void Delete();
        virtual void RequestUpdate();

        virtual void Export( AtNode* instance );

        virtual AtNode* ExportInstance(AtNode *instance, const MDagPath& masterInstance, bool update);

        virtual void ExportProcedural( AtNode *node, bool update);

        virtual void ExportUserAttrs( AtNode *node );

        virtual void ExportCurveAttrs( AtNode *node );

        virtual bool RequiresMotionData();

        virtual void ExportMotion( AtNode *node );

        static void nodeInitialiser( CAbTranslator context );

        static void *creator()
        {
                return new GpuCacheTranslator();
        }

protected :

        void GetDisplacement(MObject& obj,
                             float& dispPadding,
                             bool& enableAutoBump);

        /// Returns the arnold shader assigned to the procedural. This duplicates
        /// code in GeometryTranslator.h, but there's not much can be done about that
        /// since the GeometryTranslator isn't part of the MtoA public API.
        AtNode *arnoldShader(AtNode* node);

protected :
        MFnDagNode m_DagNode;
        bool m_isMasterDag;
        bool m_displaced;
        float m_dispPadding;
        MDagPath m_dagPathRef;
        MDagPath m_masterDag;
        AtNode* m_dispNode;
};





