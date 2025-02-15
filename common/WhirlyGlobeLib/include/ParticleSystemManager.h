/*
 *  ParticleSystemManager.h
 *  WhirlyGlobeLib
 *
 *  Created by Steve Gifford on 4/26/15.
 *  Copyright 2011-2019 mousebird consulting.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#import <math.h>
#import <set>
#import <map>
#import "Identifiable.h"
#import "BasicDrawable.h"
#import "Scene.h"
#import "SelectionManager.h"
#import "ParticleSystemDrawableBuilder.h"

namespace WhirlyKit
{
#define kWKParticleSystemManager "WKParticleSystemManager"

typedef enum {ParticleSystemPoint,ParticleSystemRectangle} ParticleSystemType;

/// Defines the parameters for a single particle system
class ParticleSystem : public Identifiable
{
public:
    ParticleSystem();
    virtual ~ParticleSystem();
    
    bool enable;
    std::string name;
    int64_t drawOrder;
    int drawPriority;
    float pointSize;
    ParticleSystemType type;
    SimpleIdentity calcShaderID;
    SimpleIdentity renderShaderID;
    TimeInterval lifetime,baseTime;
    int totalParticles,batchSize,vertexSize;
    bool continuousUpdate;
    bool zBufferRead,zBufferWrite;
    SimpleIdentity renderTargetID;
    std::vector<SingleVertexAttributeInfo> vertAttrs;
    std::vector<SingleVertexAttributeInfo> varyingAttrs;
    std::vector<SimpleIdentity> varyNames;
    std::vector<SimpleIdentity> texIDs;
    std::vector<RawDataRef> partData;  // Used for Metal particles
};

/// Holds the data for a batch of particles
class ParticleBatch
{
public:
    ParticleBatch();
    virtual ~ParticleBatch();
    
    // Should match the particle systems batch size
    int batchSize;
    // For OpenGL ES: One entry per vertex attribute.  Order corresponds to the vertAttrs array in the ParticleSystem.
    std::vector<const void *> attrData;
    // For Metal: We just pass through a mass of data without looking at it.
    RawDataRef data;
};

/// Used to track resources associated with a particle system
class ParticleSystemSceneRep : public Identifiable
{
public:
    ParticleSystemSceneRep();
    ParticleSystemSceneRep(SimpleIdentity inId);
    ~ParticleSystemSceneRep();
    
    void clearContents(ChangeSet &changes);
    void enableContents(bool enable,ChangeSet &changes);
    
    ParticleSystem partSys;
    // We have a special drawable for OpenGL
    std::set<ParticleSystemDrawable *> draws;
    // For Metal we just use instances
    SimpleIDSet basicIDs;
    SimpleIDSet instIDs;
};
    
typedef std::map<SimpleIdentity,ParticleSystemSceneRep *> ParticleSystemSceneRepSet;
    
/// Particle system manager controls the active particle systems
class ParticleSystemManager : public SceneManager
{
public:
    ParticleSystemManager();
    virtual ~ParticleSystemManager();
    
    /// Add a particle system
    SimpleIdentity addParticleSystem(const ParticleSystem &newSystem,ChangeSet &changes);
    
    /// Add a batch of particles
    void addParticleBatch(SimpleIdentity,const ParticleBatch &batch,ChangeSet &changes);
    
    /// Enable/disable active particle system
    void enableParticleSystem(SimpleIdentity sysID,bool enable,ChangeSet &changes);
    
    /// Remove a particle system referred to by the given ID
    void removeParticleSystem(SimpleIdentity sysID,ChangeSet &changes);
    
    /// Change the render target
    void changeRenderTarget(SimpleIdentity sysID,SimpleIdentity targetID,ChangeSet &changes);

    /// Apply the given uniform block to the particle systems selected
    void setUniformBlock(const SimpleIDSet &partSysIDs,const RawDataRef &uniBlock,int bufferID,ChangeSet &changes);

protected:
    ParticleSystemSceneRepSet sceneReps;
};
typedef std::shared_ptr<ParticleSystemManager> ParticleSystemManagerRef;
    
}
