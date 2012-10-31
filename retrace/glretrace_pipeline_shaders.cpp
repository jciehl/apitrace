
#include <vector>
#include <string>

#include "glproc.hpp"
#include "glsize.hpp"
#include "os.hpp"
#include "glretrace_pipeline_shaders.hpp"


namespace glretrace {

namespace pipelineview {

const float Transform::Id[4][4]= {
    {1.f, 0.f, 0.f, 0.f},
    {0.f, 1.f, 0.f, 0.f},
    {0.f, 0.f, 1.f, 0.f},
    {0.f, 0.f, 0.f, 1.f},
};
    
static
std::vector<GLuint> shaderManager;

GLuint createShader( const GLenum type ) {
    GLuint shader= glCreateShader(type);
    if(shader > 0) {
        shaderManager.push_back(shader);
    }
    
    return shader;
}

void cleanupShaders( ) {
    const int count= (int) shaderManager.size();
    for(int i= 0; i < count; i++)
        glDeleteShader(shaderManager[i]);
    
    shaderManager.clear();
}


GLuint createShader( const GLenum type, const char *source ) {
    if(source == NULL) {
        return 0;
    }
    
    GLuint shader= createShader(type);
    const char *sources= source;
    glShaderSource(shader, 1, &sources, NULL);
    glCompileShader(shader);

    GLint code;
    glGetShaderiv(shader, GL_COMPILE_STATUS,  &code);
    if(code == GL_TRUE) {
        return shader;
    }
    
    GLint length= 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if(length == 0) {
        os::log("error compiling shader (no info log).\n");
    } else {
        std::vector<GLchar> log(length, 0);
        glGetShaderInfoLog(shader, (GLsizei) length, NULL, &log.front());
        os::log("error compiling shader:\n%s\nfailed.\n", &log.front());
    }
    
    return 0;
}


static
std::vector<GLuint> programManager;

GLuint createProgram( ) {
    GLuint program= glCreateProgram();
    if(program > 0) {
        programManager.push_back(program);
    }
    
    return program;
}

void cleanupPrograms( ) {
    const int count= (int) programManager.size();
    for(int i= 0; i < count; i++) {
        glDeleteProgram(programManager[i]);
    }
    
    programManager.clear();
}


GLuint createProgram( const GLuint vertex, const GLuint fragment ) {
    if(vertex == 0 || fragment == 0) {
        return 0;
    }
    
    GLuint program= createProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    if(linkProgram(program)) {
        return program;
    }
    
    return 0;
}

GLuint createProgram( const char *vertex, const char *fragment ) {
    GLuint vertexShader= createShader(GL_VERTEX_SHADER, vertex);
    GLuint fragmentShader= createShader(GL_FRAGMENT_SHADER, fragment);
    
    return createProgram(vertexShader, fragmentShader);
}

bool linkProgram( const GLuint program ) {
    glLinkProgram(program);
    
    GLint code;
    glGetProgramiv(program, GL_LINK_STATUS, &code);
    if(code == GL_TRUE) {
        return true;
    }
    
    GLint length= 0;
    glGetShaderiv(program, GL_INFO_LOG_LENGTH, &length);
    if(length == 0) {
        os::log("error linking shader program (no info log).\n");
        
        // display source
        GLint count= 0;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &count);
        std::vector<GLuint> shaders(count, 0);
        glGetAttachedShaders(program, count, NULL, &shaders.front());
        for(int i= 0; i < count; i++) {
            GLint length= 0;
            glGetShaderiv(shaders[i], GL_SHADER_SOURCE_LENGTH, &length);
            
            GLchar *source= new GLchar[length];
            glGetShaderSource(shaders[i], length, NULL, source);
            
            os::log("shader %d:\n%s\n--\n", shaders[i], source);
            delete [] source;
        }
    } else {
        std::vector<GLchar> log(length, 0);
        glGetProgramInfoLog(program, (GLsizei) length, NULL, &log.front());
        os::log("error linking shader program:\n%s\nfailed.\n", &log.front());
    }
    
    return false;
}


static 
bool assignUniformuiv( const GLint location, const int size, const GLenum type, const void *data ) {
    switch(type) {
        case GL_UNSIGNED_INT:
            glUniform1uiv(location, size, (const GLuint *) data);
            break;
        case GL_UNSIGNED_INT_VEC2:
            glUniform2uiv(location, size, (const GLuint *) data);
            break;
        case GL_UNSIGNED_INT_VEC3:
            glUniform3uiv(location, size, (const GLuint *) data);
            break;
        case GL_UNSIGNED_INT_VEC4:
            glUniform4uiv(location, size, (const GLuint *) data);
            break;
        
        default:
            os::log("unknown type\n");
            return false;
    }
    
    return true;
}

static 
bool assignUniformiv( const GLint location, const int size, const GLenum type, const void *data ) {
    switch(type) {
        case GL_INT:
        case GL_BOOL:
        case GL_SAMPLER_1D:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_RECT:
        case GL_INT_SAMPLER_1D:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT:
            glUniform1iv(location, size, (const GLint *) data);
            break;
        
        case GL_UNSIGNED_INT_VEC2:
            glUniform2iv(location, size, (const GLint *) data);
            break;
        case GL_UNSIGNED_INT_VEC3:
            glUniform3iv(location, size, (const GLint *) data);
            break;
        case GL_UNSIGNED_INT_VEC4:
            glUniform4iv(location, size, (const GLint *) data);
            break;
        
        default:
            os::log("unknown type\n");
            return false;
    }
    
    return true;
}

static 
bool assignUniformfv( const GLint location, const int size, const GLenum type, const void *data ) {
    switch(type) {
        case GL_FLOAT:
            glUniform1fv(location, size, (const GLfloat *) data);
            break;
        case GL_FLOAT_VEC2:
            glUniform2fv(location, size, (const GLfloat *) data);
            break;
        case GL_FLOAT_VEC3:
            glUniform3fv(location, size, (const GLfloat *) data);
            break;
        case GL_FLOAT_VEC4:
            glUniform4fv(location, size, (const GLfloat *) data);
            break;
        
        case GL_FLOAT_MAT2:
            glUniformMatrix2fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT3:
            glUniformMatrix3fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT4:
            glUniformMatrix4fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        
        case GL_FLOAT_MAT2x3:
            glUniformMatrix2x3fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT2x4:
            glUniformMatrix2x4fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT3x2:
            glUniformMatrix3x2fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT3x4:
            glUniformMatrix3x4fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT4x2:
            glUniformMatrix4x2fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        case GL_FLOAT_MAT4x3:
            glUniformMatrix4x3fv(location, size, GL_FALSE, (const GLfloat *) data);
            break;
        
        default:
            os::log("unknown type\n");
            return false;
    }
    
    return true;
}

bool assignProgramUniforms( const GLint program, const GLint activeProgram ) {
    GLint count= 0;
    glGetProgramiv(activeProgram, GL_ACTIVE_UNIFORMS, &count);
    
    GLint length= 0;
    glGetProgramiv(activeProgram, GL_ACTIVE_UNIFORM_MAX_LENGTH, &length);
    
    std::vector<GLchar> name(length, 0);
    std::vector<unsigned char> data;
    for(int i= 0; i < count; i++) {
        GLint arraySize= 0;
        GLenum glslType= 0;
        glGetActiveUniform(activeProgram, i, length, NULL, &arraySize, &glslType, &name.front());
        
        GLint location= glGetUniformLocation(program, &name.front());
        if(location < 0) {
            // skip uniforms not used in required pipeline stages
            continue;
        }
        
        if(arraySize > 1) {
            //! \todo
            os::log("uniform '%s' is an array (size %d), not implemented.\n", &name.front(), arraySize);
        }
        
        // resize temp buffer to store uniform values
        GLenum itemType;
        GLint numCols, numRows;
        _gl_uniform_size(glslType, itemType, numCols, numRows);
        if(itemType == GL_NONE) {
            os::log("oops\n");
            return false;
        }
        
        if(_gl_type_size(itemType) == 0) {
            os::log("oops\n");
            return false;
        }
        
        data.clear();
        data.resize( _gl_type_size(itemType) * numRows * numCols * arraySize);
        switch(glslType) {
            case GL_UNSIGNED_INT:
            case GL_UNSIGNED_INT_VEC2:
            case GL_UNSIGNED_INT_VEC3:
            case GL_UNSIGNED_INT_VEC4:
                // unsigned
                glGetUniformuiv(activeProgram, i, (GLuint *) &data.front());
                assignUniformuiv(location, 1, glslType, &data.front());
                break;
            
            case GL_INT:
            case GL_INT_VEC2:
            case GL_INT_VEC3:
            case GL_INT_VEC4:
            case GL_BOOL:
            case GL_BOOL_VEC2:
            case GL_BOOL_VEC3:
            case GL_BOOL_VEC4:
            case GL_SAMPLER_1D:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_3D:
            case GL_SAMPLER_CUBE:
            case GL_SAMPLER_1D_ARRAY:
            case GL_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_2D_RECT:
            case GL_INT_SAMPLER_1D:
            case GL_INT_SAMPLER_2D:
            case GL_INT_SAMPLER_3D:
            case GL_INT_SAMPLER_CUBE:
            case GL_INT_SAMPLER_1D_ARRAY:
            case GL_INT_SAMPLER_2D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_1D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_3D:
            case GL_UNSIGNED_INT_SAMPLER_CUBE:
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
                // int + bool + sampler
                glGetUniformiv(activeProgram, i, (GLint *) &data.front());
                assignUniformiv(location, 1, glslType, &data.front());
                break;
                
            case GL_FLOAT:
            case GL_FLOAT_VEC2:
            case GL_FLOAT_VEC3:
            case GL_FLOAT_VEC4:
            case GL_FLOAT_MAT2:
            case GL_FLOAT_MAT3:
            case GL_FLOAT_MAT4:
            case GL_FLOAT_MAT2x3:
            case GL_FLOAT_MAT2x4:
            case GL_FLOAT_MAT3x2:
            case GL_FLOAT_MAT3x4:
            case GL_FLOAT_MAT4x2:
            case GL_FLOAT_MAT4x3:
                // float + matrix
                glGetUniformfv(activeProgram, i, (GLfloat *) &data.front());
                assignUniformfv(location, 1, glslType, &data.front());
                break;
            
            default:
                os::log("unknown type\n");
                break;
        }
    }
    
    //! \todo add support for subroutine uniforms
    //! \todo add support for uniform block bindings
    //! \todo add support for storage block bindings
    return true;
}


}       // namespace pipeline view

}       // namespace glretrace
