
#include "glproc.hpp"
#include "glretrace.hpp"
#include "glsize.hpp"


namespace glretrace {

namespace pipelineview {        // isolate global variables

static Context *pipelineContext= NULL;

bool initContext( ) {
    if(pipelineContext != NULL) {
        return true;
    }
    
    pipelineContext= createContext(currentContext);
    return (pipelineContext != NULL);
}

bool useContext( ) {
    if(pipelineContext == NULL) {
        return false;
    }
    
    bool code=glws::makeCurrent(currentDrawable, pipelineContext->wsContext);
    if(code) {
        GLint major= 0, minor= 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        int version= major * 100 + minor *10;
        std::cerr << "pipeline view openGL version " << version << "\n";
        if(version < 320) {
            std::cerr << "pipeline-view openGL version " << version << "is not supported.\n";
            // pipeline view requires transform feedback
            exit(1);
        }
    }
    
    return code;
}

bool restoreContext( ) {
    if(currentContext == NULL) {
        return false;
    }
    
    return glws::makeCurrent(currentDrawable, currentContext->wsContext);
}


// pipeline stage ids
GLenum shaderTypes[]= {
    GL_VERTEX_SHADER, 
    GL_TESS_CONTROL_SHADER,
    GL_TESS_EVALUATION_SHADER,
    GL_GEOMETRY_SHADER,
    GL_FRAGMENT_SHADER,
    0
};

const char* shaderTypeNames[]= {
    "vertex",
    "control",
    "evaluation",
    "geometry",
    "fragment",
    NULL
};

enum {
    VERTEX_STAGE_BIT= 1,
    CONTROL_STAGE_BIT= 2,
    EVALUATION_STAGE_BIT= 4,
    GEOMETRY_STAGE_BIT= 8,
    FRAGMENT_STAGE_BIT= 16,
    MAX_STAGES= 5
};

#define TRANSFORM_STAGES_MASK (VERTEX_STAGE_BIT | CONTROL_STAGE_BIT | EVALUATION_STAGE_BIT | GEOMETRY_STAGE_BIT)
#define TESSELATION_STAGES_MASK (CONTROL_STAGE_BIT | EVALUATION_STAGE_BIT)


unsigned int shaderStages[]= {
    VERTEX_STAGE_BIT,
    CONTROL_STAGE_BIT,
    EVALUATION_STAGE_BIT,
    GEOMETRY_STAGE_BIT,
    FRAGMENT_STAGE_BIT
};


GLint activeProgram= 0;
std::vector<GLuint> activeShaders;
GLint activeShaderCount= 0;


int getActiveStages( )
{
    activeProgram= 0;
    activeShaderCount= 0;
    activeShaders.clear();
    activeShaders.resize(MAX_STAGES);
    
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if(activeProgram == 0) {
        std::cerr << "no shader program.\n";
        return -1;      // no program
    }
    
    //! \todo add support for program pipelines. 
    
    GLint linked;
    glGetProgramiv(activeProgram, GL_LINK_STATUS, &linked);
    if(linked == GL_FALSE) {
        std::cerr << "shader program is not linked. can't display stages.\n";
        return -1;      // program can't run
    }
    
    glGetProgramiv(activeProgram, GL_ATTACHED_SHADERS, &activeShaderCount);
    if(activeShaderCount == 0) {
        std::cerr << "shader program has no shader objects attached, can't display stages.\n";
        return -1;      // no shaders
    }
    
    GLint count= 0;
    std::vector<GLuint> shaders(activeShaderCount, 0);
    glGetAttachedShaders(activeProgram, activeShaderCount, &count, &shaders.front());
    
    std::cerr << "shader program object " << activeProgram << ":\n";
    for(int i= 0; i < count; i++) {
        GLint type;
        glGetShaderiv(shaders[i], GL_SHADER_TYPE, &type);
        
        int stage= 0;
        const char *typeName= "<oops>";
        for(stage= 0; stage < MAX_STAGES; stage++) {
            if(shaderTypes[stage] != (GLenum) type)
                continue;
            typeName= shaderTypeNames[stage];
            break;
        }
        std::cerr << "  " << typeName << " shader object " << shaders[i] << "\n", 

        // assert shader_stages[] order.
        activeShaders[stage]= shaders[i];
    }
    
    std::cerr << "  done.\n";
    return 0;
}

GLuint findActiveShader( const GLenum type )
{
    for(int i= 0; i < MAX_STAGES; i++) {
        if(shaderTypes[i] == type)
            return activeShaders[i];
    }
    
    std::cerr << "unknown shader type.\n";
    return 0;
}


struct Attribute
{
    std::vector<GLchar> name;
    GLint arraySize;
    GLint glslType;
};

std::vector<Attribute> activeAttributes;
GLint activeAttributeCount= 0;

int getActiveAttributes( )
{
    activeAttributeCount= 0;
    activeAttributes.clear();

    if(activeShaderCount == 0) {
        return -1;      // no program
    }
    
    glGetProgramiv(activeProgram, GL_ACTIVE_ATTRIBUTES, &activeAttributeCount);
    activeAttributes.resize(activeAttributeCount);
    
    std::cerr << activeAttributeCount << " required attributes:\n";
    
    GLint attributeLength= 0;
    glGetProgramiv(activeProgram, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attributeLength);
    
    GLint size;
    GLenum glslType;
    for(int i= 0; i < activeAttributeCount; i++) {
        activeAttributes[i].name.clear();
        activeAttributes[i].name.resize(attributeLength);
        glGetActiveAttrib(activeProgram, i, attributeLength, NULL, &size, &glslType, &activeAttributes[i].name.front());
        std::cerr << "  attribute " << i << " '" << &activeAttributes[i].name.front() << "': array size " << size << " glslType " << glslType << "\n";
        
        activeAttributes[i].arraySize= size;
        activeAttributes[i].glslType= glslType;
    }
    
    std::cerr << "  done.\n";
    return 0;
}


struct BufferBinding
{
    GLint buffer;
    GLint enabled;
    GLint size;
    GLint type;
    GLint stride;
    GLint glslType;
    GLint normalized;
    GLint integer;
    GLint divisor;
    GLint64 length;
    GLint64 offset;
};

std::vector<BufferBinding> activeBuffers;
GLint activeVertexArray= 0;
GLint activeVertexBuffer= 0;
GLint activeIndexBuffer= 0;

int getActiveBuffers( )
{
    activeVertexBuffer= 0;
    activeIndexBuffer= 0;
    activeVertexArray= 0;
    activeBuffers.clear();
    
    if(activeAttributeCount == 0) {
        return -1;
    }
    
    std::cerr << "active buffers:\n";
    
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &activeVertexBuffer);
    if(activeVertexBuffer == 0) {
        std::cerr << "  no vertex buffer object\n";
    } else {
        std::cerr << "  vertex buffer object " << activeVertexBuffer << "\n";
    }
    
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &activeIndexBuffer);
    if(activeIndexBuffer == 0) {
        std::cerr << "  no index buffer object\n";
    } else {
        std::cerr <<"  index buffer object " << activeIndexBuffer << "\n";
    }
    
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &activeVertexArray);
    if(activeVertexArray == 0) {
        std::cerr << "  no vertex array object\n";
    } else {
        std::cerr << "  vertex array object " << activeVertexArray << "\n";
    }
    
    bool failed= false;
    activeBuffers.resize(activeAttributeCount);
    for(int i= 0; i < activeAttributeCount; i++) {
        GLint buffer= 0;
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
        if(buffer == 0) {
            std::cerr << "no vertex buffer bound to attribute " << i << " '" << &activeAttributes[i].name.front() << "'\n" ;
            failed= true;
            continue;
        }
        
        GLint size, type, stride;
        GLint enabled, normalized;
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
        if(stride == 0)
            stride= _gl_type_size(type) * size;
        
        // determine glsl type from (size, type)
        GLint glslType= type;  // not used
        if(size > 1) {
            switch(type) {
            case GL_FLOAT:
                glslType= GL_FLOAT_VEC2 + size-2;
                break;
            case GL_DOUBLE:
                glslType= GL_DOUBLE_VEC2 + size-2;
                break;
            case GL_INT:
                glslType= GL_INT_VEC2 + size-2;
                break;
            case GL_UNSIGNED_INT:
                glslType= GL_UNSIGNED_INT_VEC2 + size-2;
                break;
            //!\todo more types
            }
        }
        
        GLvoid *offset= NULL;
        glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &offset);
        
        GLint64 length= 0;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &length);
        
        activeBuffers[i].buffer= buffer;
        activeBuffers[i].enabled= enabled;
        activeBuffers[i].size= size;
        activeBuffers[i].type= type;
        activeBuffers[i].glslType= glslType;
        activeBuffers[i].normalized= normalized;
        activeBuffers[i].stride= stride;
        activeBuffers[i].length= length;
        activeBuffers[i].offset= (GLint64) offset;
        
        if(buffer != 0) {
            if(activeProgram != 0) {
                //! \todo use std::cerr 
                fprintf(stderr, "    attribute %d '%s': vertex buffer object %d, enabled %d, item size %d, item type 0x%x, glsl type 0x%x, stride %d, offset %lu\n", 
                    i, &activeAttributes[i].name.front(), 
                    buffer, enabled, size, type, glslType, stride, (GLint64) offset);
            } else {
                fprintf(stderr, "    attribute %d: vertex buffer object %d, enabled %d, item size %d, item type 0x%x, glsl type 0x%0x, stride %d, offset %lu\n", 
                    i, buffer, 
                    enabled, size, type, glslType, stride, (GLint64) offset);
            }
        }
    }
    
    // restore state
    glBindBuffer(GL_ARRAY_BUFFER, activeVertexBuffer);
    if(failed) {
        std::cerr << "  failed.\n";
    } else {
        std::cerr << "  done.\n";
    }
    return 0;
}



}       // namespace pipelineview
}       // namespace glretrace


namespace retrace {

void pipelineView( trace::Call* call )
{
    std::cerr << "pipeline-view enter\n";
    
    glretrace::pipelineview::getActiveStages();
    glretrace::pipelineview::getActiveAttributes();
    glretrace::pipelineview::getActiveBuffers();
    
    if(glretrace::pipelineview::initContext() == false || glretrace::pipelineview::useContext() == false) {
        glretrace::pipelineview::restoreContext();
        std::cerr << "error creating pipeline view context.\n";
        return;
    }
    
    
    glretrace::pipelineview::restoreContext();
    std::cerr << "pipeline-view leave\n";
    return;
}


}       // namespace retrace

