
#include "glproc.hpp"
#include "glretrace.hpp"


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
        return;
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


struct BufferBinding
{
    GLint buffer;
    GLint enabled;
    GLint size;
    GLint type;
    GLint stride;
    GLint glsl_type;
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

struct Attribute
{
    std::vector<GLchar> name;
    GLint array_size;
    GLint glsl_type;
};

std::vector<Attribute> activeAttributes;
GLint activeAttributeCount= 0;


int getActiveStages( )
{
    activeProgram= 0;
    activeShaderCount= 0;
    activeShaders.clear();
    activeShaders.resize(MAX_STAGES);
    
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if(activeProgram == 0)
    {
        std::cerr << "no shader program.\n";
        return -1;      // no program
    }
    
    //! \todo add support for program pipelines. 
    
    GLint linked;
    glGetProgramiv(activeProgram, GL_LINK_STATUS, &linked);
    if(linked == GL_FALSE)
    {
        std::cerr << "shader program is not linked. can't display stages.\n";
        return -1;      // program can't run
    }
    
    glGetProgramiv(activeProgram, GL_ATTACHED_SHADERS, &activeShaderCount);
    if(activeShaderCount == 0)
    {
        std::cerr << "shader program has no shader objects attached, can't display stages.\n";
        return -1;      // no shaders
    }
    
    GLint count= 0;
    std::vector<GLuint> shaders(activeShaderCount, 0);
    glGetAttachedShaders(activeProgram, activeShaderCount, &count, &shaders.front());
    
    std::cout << "shader program object " << activeProgram << ":\n";
    for(int i= 0; i < count; i++)
    {
        GLint type;
        glGetShaderiv(shaders[i], GL_SHADER_TYPE, &type);
        
        int stage= 0;
        const char *typeName= "<oops>";
        for(stage= 0; stage < MAX_STAGES; stage++)
        {
            if(shaderTypes[stage] != (GLenum) type)
                continue;
            typeName= shaderTypeNames[stage];
            break;
        }
        std::cout << "  " << typeName << " shader object " << shaders[i] << "\n", 

        // assert shader_stages[] order.
        activeShaders[stage]= shaders[i];
    }
    
    std::cout << "  done.\n";
    return 0;
}

GLuint findActiveShader( const GLenum type )
{
    for(int i= 0; i < MAX_STAGES; i++)
    {
        if(shaderTypes[i] == type)
            return activeShaders[i];
    }
    
    std::cerr << "unknown shader type.\n";
    return 0;
}


}       // namespace pipelineview
}       // namespace glretrace


namespace retrace {

void pipelineView( trace::Call* call )
{
    std::cerr << "pipeline-view enter\n";
    if(glretrace::pipelineview::initContext() == false || glretrace::pipelineview::useContext() == false) {
        std::cerr << "error creating pipeline view context.\n";
        return;
    }
    
    
    glretrace::pipelineview::restoreContext();
    std::cerr << "pipeline-view leave\n";
    return;
}


}       // namespace retrace

