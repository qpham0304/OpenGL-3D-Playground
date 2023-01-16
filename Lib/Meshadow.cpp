// Mesh.cpp - mesh IO and operations (c) 2019-2022 Jules Bloomenthal

#include "GLXtras.h"
#include "Meshadow.h"
#include "Misc.h"
#include <string.h>

// Mesh Shaders

namespace {

GLuint meshadowShader = 0;

const char *meshadowVertexShader = R"(
	#version 430
	layout (location = 0) in vec4 point;
	layout (location = 1) in vec4 normal;
	layout (location = 2) in vec4 uv;
	out vec3 vPoint;
	out vec3 vNormal;
	out vec2 vUv;
	uniform bool useInstance = false;
	uniform mat4 modelview;
	uniform mat4 persp;
	void main() {
		mat4 m = modelview; // useInstance? modelview*instance : modelview;
		vPoint = (modelview*point).xyz;
		vNormal = (modelview*normal).xyz;
		gl_Position = persp*vec4(vPoint, 1);
		vUv = vec2(uv.x, uv.y);
	}
)";

const char *meshadowPixelShader = R"(
	#version 430
	// access to shading object
	layout (std430, binding = 20) buffer Points { vec4 objPts[]; };
	layout (std430, binding = 23) buffer Triangles { int objEids[]; };
	// object being shaded
	in vec3 vPoint, vNormal;
	in vec2 vUv;
	out vec4 pColor;
	uniform float dim = 1;
	uniform bool shadowing = false;
	uniform int nObjTriangles = 0;
	uniform mat4 objTransform;
	uniform bool useLight = true;
	uniform vec3 light;
	uniform vec3 lights[20];
	uniform int nlights = 0;
	uniform vec3 defaultColor = vec3(1);
	uniform bool useDefaultColor = true;
	uniform float opacity = 1;
	uniform sampler2D textureName;
	uniform bool useTexture = false;
	uniform bool useTint = false;
	uniform bool fwdFacing = false;
	uniform bool softShadow = true;
	uniform float lightRadius = .3;
	// SHADING
	float Intensity(vec3 normalV, vec3 eyeV, vec3 point, vec3 light) {
		vec3 lightV = normalize(light-point);		// light vector
		vec3 reflectV = reflect(lightV, normalV);   // highlight vector
		float d = max(0, dot(normalV, lightV));     // one-sided diffuse
		float s = max(0, dot(reflectV, eyeV));      // one-sided specular
		return clamp(d+pow(s, 50), 0, 1);
	}
	// SHADOWING
	float cross2d(vec2 v1, vec2 v2) { return v1.x*v2.y-v1.y*v2.x; }
	vec4 PlaneFromTriangle(vec3 p1, vec3 p2, vec3 p3) {
		vec3 v1 = vec3(p2-p1), v2 = vec3(p3-p2), x = normalize(cross(v1, v2));
		return vec4(x.x, x.y, x.z, -dot(p1, x));
	}
	bool LinePlaneIntersect(vec3 p1, vec3 p2, vec4 plane, out vec3 intersection, out float alpha) {
		vec3 normal = vec3(plane.x, plane.y, plane.z), axis = vec3(p2-p1);
		float pdDot = dot(axis, normal);
		if (abs(pdDot) < .001) return false;
		alpha = (-plane.w-dot(p1, normal))/pdDot;
		intersection = p1+alpha*axis;
		return true;
	}
	int GetMajorPlane(vec4 plane) {
		float ax = abs(plane.x), ay = abs(plane.y), az = abs(plane.z);
		return ax > ay? (ax > az? 1 : 3) : (ay > az? 2 : 3);
	}
	vec2 MajorPlane(vec3 p, int mp) { return mp == 1? vec2(p.y, p.z) : mp == 2? vec2(p.x, p.z) : vec2(p.x, p.y); }
	bool CrossPositive(vec2 a, vec2 b, vec2 c) { return cross2d(vec2(b-a), vec2(c-b)) > 0; }
	bool TestInclude(vec2 test, vec2 t1, vec2 t2, vec2 t3) {
		bool c1 = CrossPositive(test, t1, t2), c2 = CrossPositive(test, t2, t3), c3 = CrossPositive(test, t3, t1);
		return c1 == c2 && c2 == c3;
	}
	bool LineTriangleIntersect(vec3 a, vec3 b, vec3 p1, vec3 p2, vec3 p3) {
		// does line between a and b intersect triangle p1p2p3?
		vec3 intersection;
		float alpha = 0;
		vec4 plane = PlaneFromTriangle(p1, p2, p3);
		if (!LinePlaneIntersect(a, b, plane, intersection, alpha)) return false;
		if (alpha < 0 || alpha > 1) return false;
		int mp = GetMajorPlane(plane);
		return TestInclude(MajorPlane(intersection, mp), MajorPlane(p1, mp), MajorPlane(p2, mp), MajorPlane(p3, mp));
	}
	float randX(vec2 v) { return fract(sin(dot(v, vec2(12.9898, 78.233)))*43758.5453); }
	float randY(vec2 v) { return fract(sin(dot(v, vec2(912.9898, 978.233)))*943758.5453); }
	float randZ(vec2 v) { return fract(sin(dot(v, vec2(6912.0002, 6978.233)))*6943758.3545); }
	bool InShadow(vec3 l) {
		vec3 v = normalize(l-vPoint);
		vec3 p = vPoint+.0001*v;					// avoid self-blocking
		for (int i = 0; i < nObjTriangles; i++) {
			int id1 = objEids[3*i], id2 = objEids[3*i+1], id3 = objEids[3*i+2];
			vec4 p1 = objTransform*objPts[id1], p2 = objTransform*objPts[id2], p3 = objTransform*objPts[id3];
			if (LineTriangleIntersect(p, l, vec3(p1), vec3(p2), vec3(p3)))
				return true;
		}
		return false;
	}
	float ShadowFactor() {
		float factor = InShadow(light)? .5 : 1;
		int nRandomLights = softShadow? 15 : 0;
		for (int i = 0; i < nRandomLights; i++) {
			vec2 q = (i+1)*vec2(gl_FragCoord);
			vec3 l = light+lightRadius*normalize(vec3(randX(q), randY(q), randZ(q)));
			factor += InShadow(l)? .5 : 1;
		}
		return factor/(1+nRandomLights);
	}
	// MAIN
	void main() {
		vec3 N = normalize(vNormal);				// surface normal
		if (fwdFacing && N.z < 0) discard;
		vec3 E = normalize(vPoint);					// eye vector
		float intensity = 1;
		if (useLight) {
			if (nlights == 0) intensity = Intensity(N, E, vPoint, light);
			for (int i = 0; i < nlights; i++)
				intensity += Intensity(N, E, vPoint, lights[i]);
			intensity = clamp(intensity, 0, 1);
			if (shadowing)
				intensity *= ShadowFactor();
		}
		vec3 color = useTexture? texture(textureName, vUv).rgb : useDefaultColor? defaultColor : vec3(1, 1, 1);
		if (useTexture && useTint) {
			color.r *= defaultColor.r;
			color.g *= defaultColor.g;
			color.b *= defaultColor.b;
		}
		pColor = vec4(dim*intensity*color, opacity); // 1);
	}
)";

} // end namespace

GLuint GetMeshadowShader() {
	if (!meshadowShader)
		meshadowShader = LinkProgramViaCode(&meshadowVertexShader, &meshadowPixelShader);
	return meshadowShader;
}

GLuint UseMeshadowShader() {
	GLuint s = GetMeshadowShader();
	glUseProgram(s);
	return s;
}

void SetVertexBuffer(ShaderStorage &ss, GLuint index, vector<vec4> &attrib) {
	// bind a buffer object to an indexed buffer target (GL_SHADER_STORAGE_BUFFER)
	// index: binding point index within the array specified by target
	// buffer: name of a buffer object to bind to the specified binding point
	ss.binding = index;
	glGenBuffers(1, &ss.buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, ss.binding, ss.buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, attrib.size()*sizeof(vec4), attrib.data(), GL_DYNAMIC_DRAW);
}

void Meshadow::Buffer(int bindingOffset) {
	int nPoints = points.size(), nNormals = normals.size(), nUvs = uvs.size();
	vector<vec4> tempPoints(nPoints), tempNormals(nNormals), tempUvs(nUvs);
	for (int i = 0; i < nPoints; i++) tempPoints[i] = vec4(points[i], 1);
	for (int i = 0; i < nNormals; i++) tempNormals[i] = vec4(normals[i], 0);
	for (int i = 0; i < nUvs; i++) tempUvs[i] = vec4(uvs[i], 0, 1);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	SetVertexBuffer(pos, 0+bindingOffset, tempPoints);
	SetVertexBuffer(nrm, 1+bindingOffset, tempNormals);
	SetVertexBuffer(uv, 2+bindingOffset, tempUvs);
	eid.binding = 3+bindingOffset;
	glGenBuffers(1, &eid.buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, eid.binding, eid.buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, triangles.size()*sizeof(int3), triangles.data(), GL_DYNAMIC_DRAW);
}

void Meshadow::Display(CameraAB camera, bool lines) {
	int nTris = triangles.size(), nQuads = quads.size();
	bool useTexture = textureUnit > 0 && uvs.size() > 0;
	// enable shader and vertex array object
	int shader = UseMeshadowShader();
	glBindBuffer(GL_ARRAY_BUFFER, pos.buffer);
	VertexAttribPointer(shader, "point", 4, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, nrm.buffer);
	VertexAttribPointer(shader, "normal", 4, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, uv.buffer);
	VertexAttribPointer(shader, "uv", 4, 0, 0);
	// texture
	SetUniform(shader, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0+textureName);   // Unit? active texture corresponds with textureUnit or textureName?
		glBindTexture(GL_TEXTURE_2D, textureName);  // bound texture and shader id correspond with textureName
		SetUniform(shader, "textureName", (int) textureName);
	}
	// set custom transform and draw (xform = mesh transforms X view transform)
	SetUniform(shader, "modelview", camera.modelview*transform);
	SetUniform(shader, "persp", camera.persp);
	if (lines) {
		for (int i = 0; i < nTris; i++)
			glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, &triangles[i]);
		for (int i = 0; i < nQuads; i++)
			glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_INT, &quads[i]);
	}
	else {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eid.buffer);
		glDrawElements(GL_TRIANGLES, 3*nTris, GL_UNSIGNED_INT, 0); // triangles.data());
#ifdef GL_QUADS
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDrawElements(GL_QUADS, 4*nQuads, GL_UNSIGNED_INT, quads.data());
#endif
	}
	glBindVertexArray(0);
}

bool Meshadow::Read(std::string objFile, mat4 *m, bool normalize, int bindingOffset) {
	if (!ReadAsciiObj((char *) objFile.c_str(), points, triangles, &normals, &uvs, NULL, NULL, &quads)) {
		printf("Meshadow.Read: can't read %s\n", objFile.c_str());
		return false;
	}
	objFilename = objFile;
	if (normalize)
		Normalize(points, 1);
	Buffer(bindingOffset);
	if (m)
		transform = *m;
	return true;
}

bool Meshadow::Read(std::string objFile, std::string texFile, int texUnit, mat4 *m, bool normalize, int bindingOffset) {
	if (!Read(objFile, m, normalize, bindingOffset))
		return false;
	if (!texUnit) {
		printf("Meshadow.Read: bad texture unit\n");
		return false;
	}
	objFilename = objFile;
	texFilename = texFile;
	textureUnit = texUnit;
	textureName = LoadTexture((char *) texFile.c_str(), textureUnit);
	if (!textureName)
		printf("Meshadow.Read: bad texture name\n");
	return textureName > 0;
}
