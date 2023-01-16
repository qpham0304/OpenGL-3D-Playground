// Meshadow.h - mesh supporting shadows

#ifndef MESHADOW_HDR
#define MESHADOW_HDR

#include <glad.h>
#include <stdio.h>
#include <vector>
#include "Mesh.h"

using std::string;
using std::vector;

// Mesh Class and Operations

GLuint GetMeshadowShader();
GLuint UseMeshadowShader();

struct ShaderStorage { GLuint binding, buffer; };

class Meshadow : public Mesh {
public:
	Meshadow() { };
	~Meshadow() { };
	ShaderStorage pos, nrm, uv, eid; // points, normals, uvs, element ids
	void Buffer(int bindingOffset = 0);
		// if non-null, nrms and uvs assumed same size as pts
	void Display(CameraAB camera, bool lines = false);
	bool Read(string objFile, mat4 *m, bool normalize = true, int bindingOffset = 0);
	bool Read(string objFile, string texFile, int texUnit, mat4 *m = NULL, bool normalize = true, int bindingOffset = 0);
	bool Read(string objFile, vector<string> textList, int texUnit, mat4* m = NULL, bool normalize = true, int bindingOffset = 0);
};

#endif
