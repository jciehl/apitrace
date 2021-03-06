
#include <ostream>
#include <cstring>

#include "image.hpp"
#include "json.hpp"

#include "retrace_swizzle.hpp"
#include "glproc.hpp"
#include "glretrace.hpp"
#include "glstate.hpp"
#include "glstate_internal.hpp"
#include "glsize.hpp"

#include "glretrace_pipeline_shaders.hpp"


namespace glretrace {

namespace pipelineview {        // isolate global variables

static Context *pipelineContext= NULL;

bool initContext( ) {
    if(pipelineContext != NULL) {
        return true;
    }
    
    pipelineContext= createContext(currentContext, glws::PROFILE_CORE); // test, use debug output to track some bugs
    return (pipelineContext != NULL);
}

void debugOutput( GLenum source,  GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, void* userParam )
{
    if(severity == GL_DEBUG_SEVERITY_HIGH)
        os::log("openGL error:\n%s\n", message);
    else if(severity == GL_DEBUG_SEVERITY_MEDIUM)
        os::log("openGL warning:\n%s\n", message);
    else
        os::log("openGL message:\n%s\n", message);
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
        os::log("pipeline-view openGL version %d\n", version);
        if(version < 320) {
            os::log("pipeline-view openGL version %d is not supported.\n", version);
            // pipeline view requires transform feedback
            exit(1);
        }
        
        // test
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
        glDebugMessageCallbackARB(debugOutput, NULL);
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
static
GLenum shaderTypes[]= {
    GL_VERTEX_SHADER, 
    GL_TESS_CONTROL_SHADER,
    GL_TESS_EVALUATION_SHADER,
    GL_GEOMETRY_SHADER,
    GL_FRAGMENT_SHADER,
    0
};

static
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

static
unsigned int shaderStages[]= {
    VERTEX_STAGE_BIT,
    CONTROL_STAGE_BIT,
    EVALUATION_STAGE_BIT,
    GEOMETRY_STAGE_BIT,
    FRAGMENT_STAGE_BIT
};


static GLint activeProgram= 0;
static std::vector<GLuint> activeShaders;
static GLint activeShaderCount= 0;

bool getActiveStages( ) {
    activeProgram= 0;
    activeShaderCount= 0;
    activeShaders.clear();
    activeShaders.resize(MAX_STAGES);
    
    glGetIntegerv(GL_CURRENT_PROGRAM, &activeProgram);
    if(activeProgram == 0) {
        std::cerr << "no shader program.\n";
        return false;      // no program
    }
    
    //! \todo add support for program pipelines. 
    
    GLint linked;
    glGetProgramiv(activeProgram, GL_LINK_STATUS, &linked);
    if(linked == GL_FALSE) {
        std::cerr << "shader program is not linked. can't display stages.\n";
        return false;      // program can't run
    }
    
    glGetProgramiv(activeProgram, GL_ATTACHED_SHADERS, &activeShaderCount);
    if(activeShaderCount == 0) {
        std::cerr << "shader program has no shader objects attached, can't display stages.\n";
        return false;      // no shaders
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
    
    os::log("  done.\n");
    return true;
}

GLuint findActiveShader( const GLenum type ) {
    for(int i= 0; i < MAX_STAGES; i++) {
        if(shaderTypes[i] == type) {
            return activeShaders[i];
        }
    }
    
    os::log("unknown shader type.\n");
    return 0;
}


struct Attribute {
    std::vector<GLchar> name;
    GLint location;
    GLint arraySize;
    GLint glslType;
};

static std::vector<Attribute> activeAttributes;
static GLint activeAttributeCount= 0;

bool getActiveAttributes( ) {
    activeAttributeCount= 0;
    activeAttributes.clear();

    if(activeProgram == 0) {
        return false;      // no program
    }
    
    glGetProgramiv(activeProgram, GL_ACTIVE_ATTRIBUTES, &activeAttributeCount);
    activeAttributes.resize(activeAttributeCount);
    
    os::log("%d required attributes:\n", activeAttributeCount);
    
    GLint attributeLength= 0;
    glGetProgramiv(activeProgram, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attributeLength);
    
    GLint arraySize;
    GLenum glslType;
    for(int i= 0; i < activeAttributeCount; i++) {
        activeAttributes[i].name.clear();
        activeAttributes[i].name.resize(attributeLength);
        glGetActiveAttrib(activeProgram, i, attributeLength, NULL, &arraySize, &glslType, &activeAttributes[i].name.front());
        
        activeAttributes[i].location= glGetAttribLocation(activeProgram, &activeAttributes[i].name.front());
        activeAttributes[i].arraySize= arraySize;
        activeAttributes[i].glslType= glslType;
        
        os::log("  attribute '%s': location %d size %d type 0x%x\n", 
            &activeAttributes[i].name.front(), activeAttributes[i].location, arraySize, glslType);
    }
    
    os::log("  done.\n");
    return true;
}

int getAttributeId( const char *name ) {
    for(int i= 0; i < activeAttributeCount; i++) {
        if(strcmp(name, &activeAttributes[i].name.front()) == 0) {
            return i;
        }
    }
    
    return -1;
}

int getAttributeId( const int location ) {
    for(int i= 0; i < activeAttributeCount; i++) {
        if(activeAttributes[i].location == location) {
            return i;
        }
    }
    
    return -1;
}

struct BufferBinding {
    GLint buffer;
    GLint enabled;
    GLint size;
    GLint type;
    GLint stride;
    GLint normalized;
    GLint integer;
    GLint divisor;
    GLint64 length;
    GLint64 offset;
};

static std::vector<BufferBinding> activeBuffers;
static GLint activeVertexArray= 0;
static GLint activeVertexBuffer= 0;
static GLint activeIndexBuffer= 0;

bool getActiveBuffers( ) {
    activeVertexBuffer= 0;
    activeIndexBuffer= 0;
    activeVertexArray= 0;
    activeBuffers.clear();
    
    os::log("active buffers:\n");
    
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &activeVertexBuffer);
    if(activeVertexBuffer == 0) {
        os::log("  no vertex buffer object\n");
    } else {
        os::log("  vertex buffer object %d\n", activeVertexBuffer);
    }
    
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &activeIndexBuffer);
    if(activeIndexBuffer == 0) {
        os::log("  no index buffer object\n");
    } else {
        os::log("  index buffer object %d\n", activeIndexBuffer);
    }
    
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &activeVertexArray);
    if(activeVertexArray == 0) {
        os::log("  no vertex array object\n");
    } else {
        os::log("  vertex array object %d\n", activeVertexArray);
    }
    
    if(activeAttributeCount == 0) {
        return false;
    }
    
    bool failed= false;
    GLint count;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count);
    activeBuffers.resize(count);
    for(int location= 0; location < count; location++) {
        int id= getAttributeId(location);
        
        GLint buffer= 0;
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
        if(buffer == 0) {
            os::log("    no vertex buffer bound to attribute %d '%s'\n",
                location, (id < 0) ? "??" : &activeAttributes[id].name.front());
            failed= true;
            continue;
        }
        
        GLint size, type, stride;
        GLint enabled, normalized;
        GLint integer, divisor;
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
        if(stride == 0)
            stride= _gl_type_size(type) * size;
        
        GLvoid *offset= NULL;
        glGetVertexAttribPointerv(location, GL_VERTEX_ATTRIB_ARRAY_POINTER, &offset);
        
        GLint64 length= 0;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &length);
        
        activeBuffers[location].buffer= buffer;
        activeBuffers[location].enabled= enabled;
        activeBuffers[location].size= size;
        activeBuffers[location].type= type;
        activeBuffers[location].stride= stride;
        activeBuffers[location].normalized= normalized;
        activeBuffers[location].integer= integer;
        activeBuffers[location].divisor= divisor;
        activeBuffers[location].length= length;
        activeBuffers[location].offset= (GLint64) offset;
        
        if(buffer != 0) {
            os::log("    attribute '%s': location %d, vertex buffer object %d (item size %d, item type 0x%x, stride %d, offset %lu)\n", 
                (id < 0) ? "??" : &activeAttributes[id].name.front(), 
                location, buffer, size, type, stride, (GLint64) offset);
        }
    }
    
    // restore state
    glBindBuffer(GL_ARRAY_BUFFER, activeVertexBuffer);
    if(failed) {
        os::log("  failed.\n");
    } else {
        os::log("  done.\n");
    }
    return true;
}


static GLboolean activeCullTest= GL_FALSE;
static GLint activeCullFace= GL_BACK;
static GLint activeFrontFace= GL_CCW;

static GLboolean activeDepthTest= GL_FALSE;
static GLint activeDepthFunc= GL_LESS;
static GLint activeDepthMask= GL_TRUE;

static GLboolean activeRasterizerDiscard= GL_FALSE;
static GLint activeColorMask[4]= { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };

static GLint activePolygonModes[4]= { 0, 0 };

bool getActiveState( ) { 
    activeCullTest= glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_CULL_FACE_MODE, &activeCullFace);
    glGetIntegerv(GL_FRONT_FACE, &activeFrontFace);
    
    activeDepthTest= glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_DEPTH_FUNC, &activeDepthFunc);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &activeDepthMask);
    
    glGetIntegerv(GL_COLOR_WRITEMASK, activeColorMask);
    
    activeRasterizerDiscard= glIsEnabled(GL_RASTERIZER_DISCARD);
    
    glGetIntegerv(GL_POLYGON_MODE, activePolygonModes);
    return true;
}

static
const char *displayFragmentSource= {
"   #version 150\n\
    void main( ) {\n\
    if(gl_FrontFacing)\n\
            gl_FragColor= vec4(1.f, 1.f, 1.f, 1.f);\n\
        else\n\
            gl_FragColor= vec4(.7f, .7f, .7f, 1.f);\n\
    }\n\
"
};

GLuint createDisplayProgram( const unsigned int mask, const char *fragmentSource ) {
    if(mask == 0 || fragmentSource == NULL) {
        return 0;
    }
    
    GLuint program= createProgram();
    if(program == 0) {
        return 0;
    }
    
    // attach active shaders according to mask
    for(int stage= 0; stage < MAX_STAGES; stage++) {
        if((mask & (1<<stage)) == 0) {
            // not requested
            continue;
        }
        if(activeShaders[stage] == 0) {
            // not used by the application
            continue;
        }
        
        // attach selected shader
        glAttachShader(program, activeShaders[stage]);
    }
    
    // attach display fragment shader
    GLuint fragmentShader= createShader(GL_FRAGMENT_SHADER, fragmentSource);
    if(fragmentShader == 0) {
        os::log("error compiling display shader program. failed.\n");
        return 0;
    }
    glAttachShader(program, fragmentShader);
    
    // bind required attributes to the same locations
    // step 1: link display program
    if(!linkProgram(program)) {
        os::log("error linking display shader program. failed.\n");
        return 0;
    }
    
    // step 2: bind required attribute locations
    for(int i= 0; i < activeAttributeCount; i++) {
        GLint location= glGetAttribLocation(program, &activeAttributes[i].name.front());
        if(location >= 0) {
            glBindAttribLocation(program, activeAttributes[i].location, &activeAttributes[i].name.front());
            os::log("bind attrib location %d '%s'\n", activeAttributes[i].location, &activeAttributes[i].name.front());
        }
    }
    
    // step 3: relink display program
    if(!linkProgram(program)) {
        os::log("error linking display shader program. failed.\n");
        return 0;
    }
    
    return program;
}

// unecessary, cleanup everything each call ?
struct Program {
    GLuint name;
    std::vector<GLuint> stages;
    const char *fragmentSource;        //!< references a static string, nothing to free
    unsigned int mask;
    
    Program( )
        :
        name(0),
        stages(MAX_STAGES, 0),
        fragmentSource(NULL),
        mask(0)
    {}
    
    Program( const GLuint _name, const unsigned int _mask, const std::vector<GLuint>& _stages, const char *_fragmentSource )
        :
        name(_name),
        stages(_stages),
        fragmentSource(_fragmentSource),
        mask(_mask)
    {}
    
    ~Program( ) {}
    
    bool match( const unsigned int _mask, const std::vector<GLuint>& _stages, const char *_fragmentSource ) const
    {
        if(_mask != mask) {
            return false;
        }
        if(_fragmentSource != fragmentSource)  { // static strings required
            return false;
        }
        
        // mask and display source match, check shader objects
        if(_stages.size() != MAX_STAGES) {
            return false;
        }
        for(int i= 0; i < MAX_STAGES; i++) {
            if(stages[i] != _stages[i]) {
                return false;
            }
        }
        
        // match found
        return true;
        
        //! \todo compute a hash value from concatenated source strings to detect shader object changes.
        //! \todo use shader source cache from apitrace
    }
};


static std::vector<Program> displayPrograms;
static GLuint attributeProgram= 0;

//! program cache, retrieve an already built shader program or create a new one
GLuint getDisplayProgram( const unsigned int mask, const char *fragmentSource ) {
    // build required shader state
    std::vector<GLuint> stages(MAX_STAGES, 0);
    for(int i= 0; i < MAX_STAGES; i++) {
        if((mask & (1<<i)) != 0) {
            stages[i]= activeShaders[i];
        }
    }
    
    // look up program 
    int count= (int) displayPrograms.size();
    for(int i= 0; i < count; i++) {
        if(displayPrograms[i].match(mask, stages, fragmentSource)) {
            return displayPrograms[i].name;    // cache hit
        }
    }
    
    // cache miss, build a new program
    GLuint program= createDisplayProgram(mask, fragmentSource);
    if(program == 0) {
        os::log("error building display program\n");
        return 0;
    }
    
    // cache the new program
    displayPrograms.push_back( Program(program, mask, stages, fragmentSource) );
    return program;
}
    
int cleanupDisplayPrograms( ) {
    attributeProgram= 0;
    displayPrograms.clear();
    return 0;
}


bool setVertexAttribArray( const GLuint bindings, const GLuint program )
{
    if(bindings == 0 || program == 0) {
        return false;
    }
    
    GLint count;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
    os::log("shader program object %d: active attributes %d\n", program, count);
    glBindVertexArray(bindings);
    for(int i= 0; i < activeAttributeCount; i++) {
        int location= activeAttributes[i].location;
        glDisableVertexAttribArray(location);
        
        //~ if(glGetAttribLocation(program, &activeAttributes[i].name.front())) {
            //~ // attribute not required by this program
            //~ os::log("bind vertex attrib %d '%s', not used\n", location, &activeAttributes[i].name.front());
            //~ continue;
        //~ }
        
        os::log("bind vertex attrib %d '%s'\n", location, &activeAttributes[i].name.front());
        glBindBuffer(GL_ARRAY_BUFFER, activeBuffers[location].buffer);
        if(activeBuffers[location].integer) {
            glVertexAttribIPointer(location, activeBuffers[location].size, activeBuffers[location].type, 
                activeBuffers[location].stride, (const GLvoid *) activeBuffers[location].offset);
        } else {
            glVertexAttribPointer(location, activeBuffers[location].size, activeBuffers[location].type, 
                activeBuffers[location].normalized, 
                activeBuffers[location].stride, (const GLvoid *) activeBuffers[location].offset);
        }
        glEnableVertexAttribArray(location);
        glVertexAttribDivisor(location, activeBuffers[location].divisor);
    }
    
    if(activeIndexBuffer != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    }
    
    return true;
}


struct DrawCall {
    GLenum primitive;
    GLint first;
    GLsizei count;
    
    GLenum indexType;
    GLint64 indexOffset;
};

void draw( const DrawCall& params ) {
    if(params.indexType == 0)
        glDrawArrays(params.primitive, params.first, params.count);
    else
        glDrawElements(params.primitive, params.count, params.indexType, (const GLvoid *) params.indexOffset);
}


const char *attributeVertexSource= {
"   #version 150\n\
    uniform mat4 mvpMatrix;\n\
    in vec4 position;\n\
    out vec3 feedback;\n\
    void main( ) {\n\
        feedback= position.xyz;\n\
        gl_Position= mvpMatrix * position;\n\
    }\n\
"
};

static GLuint attributeProgramBuffer= 0;
static GLuint attributeProgramBindings= 0;

bool drawAttribute( const int location, const DrawCall& drawParams ) {
    int id= getAttributeId(location);
    if(id < 0 || location < 0 || location >= activeAttributeCount) {
        return false;
    }
    
    os::log("draw_attribute(%d '%s'):\n", location, &activeAttributes[id].name.front());
    
    if(activeBuffers[location].enabled == 0) {
        os::log("  attribute %d '%s' disabled. can't draw anything. failed.\n", 
            location, &activeAttributes[id].name.front());
        return false;
    }
    if(activeBuffers[location].divisor > 0) {
        os::log("  attribute %d '%s' is instanced (divisor %d). can't draw anything. failed.\n", 
            location, &activeAttributes[id].name.front(), activeBuffers[location].divisor);
        return false;
    }
    if(activeBuffers[location].buffer == 0 || activeBuffers[location].length == 0) {
        os::log("  attribute %d '%s' vertex buffer object %d, null length. can't draw anything. failed.\n", 
            location, &activeAttributes[id].name.front(), activeBuffers[location].buffer);
        return false;
    }
    
    // get attribute buffer content in 'standard' vec3 form
    // use a vertex shader and transform feedback to convert the attribute data 
    if(attributeProgram == 0) {
        attributeProgram= createProgram(attributeVertexSource, displayFragmentSource);
        glBindAttribLocation(attributeProgram, 0, "position");  // glsl 150 doesn't support layout(location = )
        
        const char *varyings= "feedback";
        //~ const char *varyings= "gl_Position";
        glTransformFeedbackVaryings(attributeProgram, 1, &varyings, GL_SEPARATE_ATTRIBS);
        if(!linkProgram(attributeProgram)) {
            os::log("error linking attribute display shader program. failed.\n");
            return false;
        }
    }
    if(attributeProgram == 0) {
        os::log("error building attribute display shader program. failed.\n");
        return false;
    }
    
    // resize transform feedback buffer
    if(attributeProgramBuffer == 0) {
        glGenBuffers(1, &attributeProgramBuffer);
    }
    if(attributeProgramBuffer == 0) {
        return false;
    }
    
    // resize feedback buffer, use array_buffer target (can't use transform feedback target to create the buffer on ati, fglrx 12.9)
    glBindVertexArray(0);
    GLint64 feedbackLength= 0;
    glBindBuffer(GL_ARRAY_BUFFER, attributeProgramBuffer);
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &feedbackLength);
    
    GLint64 stride= activeBuffers[location].stride;
    GLint64 count= (activeBuffers[location].length - activeBuffers[location].offset) / stride;      //! \bug be more precise 
    
    os::log("  attribute %d '%s', vertex buffer object %d (length %lu, stride %lu, offset %lu, count %ld)\n", 
        location, &activeAttributes[id].name.front(),
        activeBuffers[location].buffer, activeBuffers[location].length, stride, activeBuffers[location].offset, count);
    
    // store count vec3s
    GLint64 length= count * sizeof(float [3]);
    if(feedbackLength < length) {
        std::vector<unsigned char> zeroes(length, 0);
        glBufferData(GL_ARRAY_BUFFER, length, &zeroes.front(), GL_DYNAMIC_COPY);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // convert buffer content
    if(attributeProgramBindings == 0) {
        glGenVertexArrays(1, &attributeProgramBindings);
    }
    if(attributeProgramBindings == 0) {
        return false;
    }
    
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, attributeProgramBuffer);
    
    // bind the attribute buffer
    glBindVertexArray(attributeProgramBindings);
    glBindBuffer(GL_ARRAY_BUFFER, activeBuffers[location].buffer);
    if(activeBuffers[location].integer) {
        glVertexAttribIPointer(0, activeBuffers[location].size, activeBuffers[location].type, 
            activeBuffers[location].stride, (const GLvoid *) activeBuffers[location].offset);
    } else {
        glVertexAttribPointer(0, activeBuffers[location].size, activeBuffers[location].type, 
            activeBuffers[location].normalized, 
            activeBuffers[location].stride, (const GLvoid *) activeBuffers[location].offset);
    }
    glEnableVertexAttribArray(0);
    
    if(activeIndexBuffer != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    }
    
    // feedback: convert buffer content
    glEnable(GL_RASTERIZER_DISCARD);
    glUseProgram(attributeProgram);
    
    GLint64 first= activeBuffers[location].offset / stride;
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, first, count);
    glEndTransformFeedback();
    
    glDisable(GL_RASTERIZER_DISCARD);
    
    // read back buffer content
    Point bmin;
    Point bmax;
    Point *positions= (Point *) glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, GL_READ_ONLY);
    if(positions == NULL) {
        os::log("cant' read attribute '%s', vertex buffer object %d. failed.\n", 
            &activeAttributes[id].name.front(), activeBuffers[location].buffer);
    } else {
        // compute bbox
        bmin= Point( std::numeric_limits<float>::infinity() );
        bmax= Point( - std::numeric_limits<float>::infinity() );
        
        for(int i= 0; i < count; i++) {
            const Point &p= positions[i];
            
            bmin.x= std::min(p.x, bmin.x);
            bmin.y= std::min(p.y, bmin.y);
            bmin.z= std::min(p.z, bmin.z);
            
            bmax.x= std::max(p.x, bmax.x);
            bmax.y= std::max(p.y, bmax.y);
            bmax.z= std::max(p.z, bmax.z);
        }
        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
        
        os::log("  bbox (%f %f %f) (%f %f %f)\n", bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
    }
    
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    
    // compute a sensible transform to display the data
    float fov= 25.f;
    Point center= (bmin + bmax) * .5f;
    float radius= Distance(center, bmax);
    if(radius == 0.f) {
        os::log("can't get valid data from attribute '%s', vertex buffer object %d. failed.\n", 
            &activeAttributes[id].name.front(), activeBuffers[location].buffer);
        glBindVertexArray(0);
        return false;
    }
    
    float distance= radius / tanf(fov / 180.f * M_PI);
    Transform view= LookAt( Point(0.f, 0.f, distance), center, Vector(0.f, 1.f, 0.f) );
    Transform projection= Perspective( fov *2.f, 1.f, distance - radius, distance + radius );
    Transform mvp= projection * view;
    
    glUniformMatrix4fv( 
        glGetUniformLocation(attributeProgram, "mvpMatrix"), 
        1, GL_TRUE, mvp.matrix() );
    
    // draw the data
    glViewport(0, 0, 256, 256);
    glScissor(0, 0, 256, 256);
    
    glClearColor( .05f, .05f, .05f, 1.f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    
    draw(drawParams);
    
    glBindVertexArray(0);
    os::log("  done.\n");
    return true;
}

void cleanupAttributes( ) {
    glDeleteVertexArrays(1, &attributeProgramBindings);
    glDeleteBuffers(1, &attributeProgramBuffer);
    
    attributeProgramBindings= 0;
    attributeProgramBuffer= 0;
}

bool drawVertexStage( const DrawCall& drawParams ) {
    glViewport(256, 0, 256, 256);
    glScissor(256, 0, 256, 256);
    
    if(findActiveShader(GL_VERTEX_SHADER) == 0) {
        // nothing to do, display a solid color background ?
        glClearColor( .15f, 0.f, .15f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }
    
    os::log("draw_vertex_stage( ):\n");
    
    GLuint program= getDisplayProgram( VERTEX_STAGE_BIT, displayFragmentSource );
    if(program == 0) {
        glClearColor( 1.f, .0f, .0f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        os::log("error building vertex display shader program. failed.\n");
        return false;
    }
    
    glClearColor( .05f, .05f, .05f, 1.f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // assign uniforms 
    glUseProgram(program);
    assignProgramUniforms(program, activeProgram);
    
    // draw
    //~ glBindVertexArray(0);
    //~ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    //~ glBindVertexArray(activeVertexArray);
    setVertexAttribArray(attributeProgramBindings, program);
    //~ setVertexAttribArray(activeVertexArray, program);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    
    draw(drawParams);
    
    os::log("  done.\n");
    return 0;
}


bool drawGeometryStage( const DrawCall& drawParams ) {
    glViewport(512, 0, 256, 256);
    glScissor(512, 0, 256, 256);
    
    if(findActiveShader(GL_GEOMETRY_SHADER) == 0) {
        // nothing to do, display a solid color background ?
        glClearColor( .5f, 0.f, .5f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }

    os::log("draw_geometry_stage( ):\n");
    
    GLuint program= getDisplayProgram( TRANSFORM_STAGES_MASK, displayFragmentSource );
    if(program == 0) {
        // display a solid red background
        glClearColor( 1.f, .0f, .0f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        os::log("error building geometry display shader program. failed.\n");
        return false;
    }
    
    glClearColor( .05f, .05f, .05f, 1.f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // assign uniforms 
    glUseProgram(program);
    assignProgramUniforms(program, activeProgram);
    
    // draw
    //~ glBindVertexArray(0);
    //~ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    //~ glBindVertexArray(activeVertexArray);
    setVertexAttribArray(attributeProgramBindings, program);
    //~ setVertexAttribArray(activeVertexArray, program);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    
    draw(drawParams);
    
    os::log("  done.\n");
    return true;
}


bool drawCullingStage( const DrawCall& drawParams ) {
    glViewport(768, 0, 256, 256);
    glScissor(768, 0, 256, 256);
    
    bool todo= true;
    if(activeCullTest == GL_FALSE) {
        // nothing to do when culling is disabled
        todo= false;
    }
    
    GLuint geometry= findActiveShader(GL_GEOMETRY_SHADER);
    if(geometry != 0) {
        GLint outputType= 0;
        glGetProgramiv(activeProgram, GL_GEOMETRY_OUTPUT_TYPE, &outputType);
        if(outputType != GL_TRIANGLE_STRIP) {
            // nothing to cull when the geometry shader outputs lines or points
            todo= false;
        }
    } else {
        // check primitive mode from draw command, if no geometry shader is present
        switch(drawParams.primitive) {
            case GL_POINTS:
            case GL_LINE_STRIP:
            case GL_LINE_LOOP:
            case GL_LINES:
            case GL_LINE_STRIP_ADJACENCY:
            case GL_LINES_ADJACENCY:
                // nothing to cull when drawing lines or points
                todo= false;
                break;
        }
    }
    
    if(!todo) {
        // nothing to do, display a solid color background ?
        glClearColor( .5f, 0.f, .5f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }
    
    os::log("draw_culling_stage( ):\n");
    
    GLuint program= getDisplayProgram( TRANSFORM_STAGES_MASK, displayFragmentSource );
    if(program == 0) {
        // display a solid red background
        glClearColor( 1.f, .0f, .0f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        os::log("error building culling display shader program. failed.\n");
        return false;
    }
    
    glClearColor( .05f, .05f, .05f, 1.f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // assign uniforms 
    glUseProgram(program);
    assignProgramUniforms(program, activeProgram);
    
    // draw
    //~ glBindVertexArray(0);
    //~ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    //~ glBindVertexArray(activeVertexArray);
    setVertexAttribArray(attributeProgramBindings, program);
    //~ setVertexAttribArray(activeVertexArray, program);
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_CULL_FACE);
    //! \todo import cull state from application context
    
    draw(drawParams);
    
    os::log("  done.\n");
    return true;    
}

bool drawFragmentStage( const DrawCall& drawParams ) {
    glViewport(1024, 0, 256, 256);
    glScissor(1024, 0, 256, 256);

    if(activeRasterizerDiscard) {
        // nothing to do, nothing to rasterize, display a solid color background ?
        glClearColor( .5f, 0.f, .5f, 1.f );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return true;
    }
    
    glClearColor( .05f, .05f, .05f, 1.f );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    os::log("draw_fragment_stage( ):\n");
    
    //~ glBindVertexArray(0);
    //~ glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, activeIndexBuffer);
    //~ glBindVertexArray(activeVertexArray);
    //~ setVertexAttribArray(attributeProgramBindings, activeProgram);
    setVertexAttribArray(activeVertexArray, activeProgram);     // rebind buffer objects, shared container object...
    
    glUseProgram(activeProgram);
    glPolygonMode(GL_FRONT_AND_BACK, activePolygonModes[0]);
    if(activeCullTest == GL_FALSE) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        //! \todo import cull state from application context
    }
    if(activeDepthTest == GL_FALSE) {
        glDisable(GL_DEPTH_TEST);
    } else {
        glEnable(GL_DEPTH_TEST);
        //! \todo import depth state from application context
    }
    
    //! \todo import blend state from application context: or display the raw output from fragment shader before pixel tests/blend ?
    //! \todo test color write mask ? same remark
    
    // draw
    draw(drawParams);
    
    os::log("  done.\n");
    return true;    
}


static GLuint framebuffer= 0;
static GLuint colorTexture= 0;
static GLuint depthTexture= 0;

bool initFramebuffer( const int w, const int h ) {
    if(framebuffer > 0)
        return true;
    
    if(colorTexture == 0) {
        glGenTextures(1, &colorTexture);
        glBindTexture(GL_TEXTURE_2D, colorTexture);

        std::vector<unsigned char> zeroes(w * h * 3, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, 
            GL_RGB, w, h, 0,
            GL_RGB, GL_UNSIGNED_BYTE, &zeroes.front());
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if(colorTexture == 0) {
        return false;
    }
    
    if(depthTexture == 0) {
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);

        std::vector<unsigned char> zeroes(w * h * 4, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, 
            GL_DEPTH_COMPONENT, w, h, 0,
            GL_DEPTH_COMPONENT, GL_FLOAT, &zeroes.front());
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    if(depthTexture == 0) {
        return false;
    }
    
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

bool cleanupFramebuffer( ) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteTextures(1, &depthTexture);
    
    framebuffer= 0;
    colorTexture= 0;
    depthTexture= 0;    
    return true;
}

bool useFramebuffer( ) {
    if(framebuffer == 0) {
        return false;
    }
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    return true;
}

bool restoreFramebuffer( ) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

image::Image *getStageSnapshot( )
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    std::vector<unsigned char> pixels(1280*256*3, 0);
    glReadPixels(0, 0, 1280, 256, GL_RGB, GL_UNSIGNED_BYTE, &pixels.front());
    
    return NULL;
}


}       // namespace pipelineview
}       // namespace glretrace


namespace retrace {

void pipelineView( trace::Call* call, std::ostream& os ) {
    os::log("pipeline-view enter\n");
    if(call == NULL) {
        return;
    }

    os::log("call %d '%s':\n", call->no, call->name());
    
    if(strcmp(call->name(), "glDrawElements") != 0) {
        os::log("call %d '%s' not implemented.\n", call->no, call->name());
        return;
    }

    GLenum mode= static_cast<GLenum>((call->arg(0)).toSInt());
    GLsizei count= (call->arg(1)).toSInt();
    GLenum type= static_cast<GLenum>((call->arg(2)).toSInt());
    const GLvoid *indices= static_cast<const GLvoid *>(retrace::toPointer(call->arg(3)));
    
    // store draw call parameters
    glretrace::pipelineview::DrawCall params;
    params.primitive= mode;
    params.first= 0;
    params.count= count;
    params.indexType= type;
    params.indexOffset= (unsigned long int) indices;    
    
    glretrace::pipelineview::getActiveStages();
    glretrace::pipelineview::getActiveAttributes();
    glretrace::pipelineview::getActiveBuffers();
    glretrace::pipelineview::getActiveState();

    //retrace draw call
    os::log("draw\n");
    draw(params);

    // pipeline view
    if(!glretrace::pipelineview::initContext() || !glretrace::pipelineview::useContext()) {
        glretrace::pipelineview::restoreContext();
        os::log("error creating pipeline view context.\n");
        return;
    }
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    
    if(!glretrace::pipelineview::initFramebuffer(1280, 256) || !glretrace::pipelineview::useFramebuffer()) {
        glretrace::pipelineview::restoreContext();
        os::log("error creating pipeline view framebuffer.\n");
        return;
    }
    
    // draw stage
    int location= 0;    //! \todo default attribute (location == 0) for now, need some gui work to choose another one
    glretrace::pipelineview::drawAttribute(location, params);
    //~ {glretrace::pipelineview::getActiveAttributes(); glretrace::pipelineview::getActiveBuffers();}
    
    glretrace::pipelineview::drawVertexStage(params);
    //~ {glretrace::pipelineview::getActiveAttributes(); glretrace::pipelineview::getActiveBuffers();}
    
    glretrace::pipelineview::drawGeometryStage(params);
    //~ {glretrace::pipelineview::getActiveAttributes(); glretrace::pipelineview::getActiveBuffers();}
    
    glretrace::pipelineview::drawCullingStage(params);
    //~ {glretrace::pipelineview::getActiveAttributes(); glretrace::pipelineview::getActiveBuffers();}
    
    glretrace::pipelineview::drawFragmentStage(params);
    //~ {glretrace::pipelineview::getActiveAttributes(); glretrace::pipelineview::getActiveBuffers();}
    
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, glretrace::pipelineview::framebuffer);
    glBlitFramebuffer(0, 0, 1280, 256, 0, 0, 1280, 256, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    {
        os::log("get snapshots\n");
        JSONWriter json(os);
        glstate::Context context;
        glstate::dumpFramebuffer(json, context);
    }
    
    glretrace::pipelineview::cleanupAttributes();
    glretrace::pipelineview::cleanupPrograms();
    glretrace::pipelineview::cleanupShaders();
    glretrace::pipelineview::cleanupDisplayPrograms();
    glretrace::pipelineview::cleanupFramebuffer();
    
    glretrace::pipelineview::restoreFramebuffer();
    glretrace::pipelineview::restoreContext();
    os::log("pipeline-view leave\n");
    return;
}


}       // namespace retrace

