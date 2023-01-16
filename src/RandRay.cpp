
#include <glad.h>
#include <glfw3.h>
#include "CameraArcball.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Meshadow.h"
#include "Mesh.h"
#include "Misc.h"
#include "VecMat.h"
#include "Slider.h"
#include "float.h"	

// window, camera, mouse, light
int winWidth = 500, winHeight = 500;
CameraAB camera(0, 0, winWidth, winHeight, vec3(0, 0, 0), vec3(0, -1, -10), 30, .001f, 500);
time_t mouseEvent = -1000;
vec3 lightDefaultPos(-1.2f, 1.4f, 1.8f);
vec3 objDefaultPos(0, 1.01f * 2, 0);
vec3 objectPos(objDefaultPos);
vec3 light(lightDefaultPos);

// control variables
bool useCube = true;
float dim = 1;
float lightRadius = 0.2;
float numlight = 1;
bool faceted = true;
unsigned int rot = 0;
bool cpuShadow = false;
bool flag = false;
float shift = 0, shift1 = 0.75;


// interactions
Mover mover;
void* picked = NULL;
vec3 blk(0, 0, 0), wht(1, 1, 1), red(1, 0, 0), grn(0, 1, 0), blu(0, 0, 1), yel(1, 1, 0), pur(1, 0, 1);
Slider lightx(10, winHeight - 20, winWidth / 2, -2, 2, light.x, false, 10, "x", &red);
Slider lighty(10, winHeight - 40, winWidth / 2, 0, 3, light.y, false, 10, "y", &grn);
Slider lightz(10, winHeight - 60, winWidth / 2, -10, 10, light.z, false, 10, "z", &blu);
Slider wres(10, 20, winWidth / 2, 0.5, 10, 5, false, 10, "waveres", &blu);
Slider lightdim(10, winHeight - 80, winWidth / 2, 0, 1, 1, false, 10, "intensity");
Slider lightSize(10, winHeight - 100, winWidth / 2, 0.01, 1.0, lightRadius, false, 10, "lsize", &pur);
Slider freqControl(10, 20, winWidth / 2, 0.5, 10, 5, false, 10, "freq", &blu);
Slider* sliders[] = { &lightx, &lighty, &lightz, &lightdim, &freqControl, &lightSize };

// object and floor
Meshadow object, square, wall, wall2;
vector<string> textureNames;
const int objTextureStartIndex = 2;
int currentTexture = objTextureStartIndex;
const int objTextureEndIndex = 4;


// wavy object
Mesh wavyMesh;
float freq = 2, ampl = .3f;
int res = 15;
int selectedQuad = -1;
bool diagnostics = false;

// set of item to be loaded
string catFile = "./Assets/Cat.obj";
string cubeFile = "./Cube-Triangles.obj";
string squareFile = "./Square.obj";
string catTexFile = "./Assets/Cat.tga";
string squareTexFile = "./metalic-2.jpg";
string cubeTexFile = "./metalic-1.jpg";
string cubeTexFile1 = "./mr_wallen.png";
string cubeTexFile2 = "./function_pointer.jpg";
string cubeTexFile3 = "./thumbsup2.jpg";

// keyboard usage
const char* usage = R"(
	F: Toggle faceted
	D: Toggle wavy points disagnostics
	L: Next cube texture fdlksqer
	K: Previous cube texture
	S: Toggle shadow disagnostics
	Q: Increase rotation speed
	E: Decrease rotation speed
	R: Reset some settings to default position
	+: Increase the number of light rays
	-: Reduce the number of light rays
	0: Toggle between Wavycube and square object
	1/(shift + 1): Increase/decrease amplitude
	2/(shift + 2): Increase/decrease frequency
	3/(shift + 3): Increase/crease the cube's resolution
)";


// Intersection Test

vec4 PlaneFromTriangle(vec3 p1, vec3 p2, vec3 p3) {
	vec3 v1(p2 - p1), v2(p3 - p2), x = normalize(cross(v1, v2));
	return vec4(x.x, x.y, x.z, -dot(p1, x));
}

bool LinePlaneIntersect(vec3 p1, vec3 p2, vec4 plane, vec3& intersection, float& alpha) {
	vec3 normal(plane.x, plane.y, plane.z), axis(p2 - p1);
	float pdDot = dot(axis, normal);
	if (fabs(pdDot) < FLT_MIN) return false;
	alpha = (-plane.w - dot(p1, normal)) / pdDot;
	intersection = p1 + alpha * axis;
	return true;
}

int GetMajorPlane(vec4 plane) {
	float ax = fabs(plane.x), ay = fabs(plane.y), az = fabs(plane.z);
	return ax > ay ? (ax > az ? 1 : 3) : (ay > az ? 2 : 3);
}

vec2 MajorPlane(vec3& p, int mp) { return mp == 1 ? vec2(p.y, p.z) : mp == 2 ? vec2(p.x, p.z) : vec2(p.x, p.y); }

bool CrossPositive(vec2 a, vec2 b, vec2 c) { return cross(vec2(b - a), vec2(c - b)) > 0; }

bool TestInclude(vec2 test, vec2 t1, vec2 t2, vec2 t3) {
	bool c1 = CrossPositive(test, t1, t2), c2 = CrossPositive(test, t2, t3), c3 = CrossPositive(test, t3, t1);
	return c1 == c2 && c2 == c3;
}

bool LineTriangleIntersect(vec3 a, vec3 b, vec3 p1, vec3 p2, vec3 p3, vec3& intersection) {
	float alpha = 0;
	vec4 plane = PlaneFromTriangle(p1, p2, p3);
	if (!LinePlaneIntersect(a, b, plane, intersection, alpha)) return false;
	if (alpha < 0 || alpha > 1) return false;
	int mp = GetMajorPlane(plane);
	return TestInclude(MajorPlane(intersection, mp), MajorPlane(p1, mp), MajorPlane(p2, mp), MajorPlane(p3, mp));
}

vec3 GetBase(Mesh& m) {
	// return origin of mesh
	mat4& f = m.transform;
	return vec3(f[0][3], f[1][3], f[2][3]);
}
// Display

vec3 Xform(mat4 m, vec3 p) { vec4 v = m * vec4(p, 1); return vec3(v.x, v.y, v.z); }

bool IntersectCube(vec3 a, vec3 b) {
	vector<vec3>& pts = object.points;
	mat4& m = object.transform;
	vec3 intersection;
	// Line(a, b, 1, blu, 0.1);
	for (int i = 0; i < (int)object.triangles.size(); i++) {
		int3 t = object.triangles[i];
		vec3 p1 = Xform(m, pts[t.i1]), p2 = Xform(m, pts[t.i2]), p3 = Xform(m, pts[t.i3]);
		if (LineTriangleIntersect(a, b, p1, p2, p3, intersection))
			return true;
	}

	return false;
}

vec3 Lerp(vec3 p1, vec3 p2, float a) { return p1 + a * (p2 - p1); }

void SetWavyIndices() {
	enum loc { ltn = 0, ltf, lbf, lbn, rtn, rtf, rbf, rbn };
	int npts = wavyMesh.points.size(), nquads = (res - 1) * 4 + 2, q = 0;
	wavyMesh.quads.resize(nquads);
	for (int i = 0; i < (int)wavyMesh.points.size() - 4; i += 4) {
		wavyMesh.quads[q++] = int4(i + ltn, i + rtn, i + rtf, i + ltf); // top
		wavyMesh.quads[q++] = int4(i + lbn, i + lbf, i + rbf, i + rbn); // bottom
		wavyMesh.quads[q++] = int4(i + ltn, i + lbn, i + rbn, i + rtn); // near
		wavyMesh.quads[q++] = int4(i + ltf, i + rtf, i + rbf, i + lbf); // far
	}
	wavyMesh.quads[q++] = int4(0, 1, 2, 3);						// left
	wavyMesh.quads[q++] = int4(npts - 4, npts - 3, npts - 2, npts - 1);	// right
}


void MakeWavyPoints() {
	vec3 lbn(-1, -1, -1), lbf(-1, -1, 1), ltn(-1, 1, -1), ltf(-1, 1, 1),
		rbn(1, -1, -1), rbf(1, -1, 1), rtn(1, 1, -1), rtf(1, 1, 1);
	int npts = 4 * res, p = 0;
	wavyMesh.points.resize(npts);
	for (int i = 0; i < res; i++) {
		float s = (float)i / (res - 1);
		float angle = s * freq * 2 * 3.141582f;
		float offset = ampl * sin(angle + shift);
		float offset1 = ampl * sin(angle + shift1);
		wavyMesh.points[p++] = Lerp(ltn, rtn, s) + vec3(0, offset, 0); // tn
		wavyMesh.points[p++] = Lerp(ltf, rtf, s) + vec3(0, offset1, 0); // tf
		wavyMesh.points[p++] = Lerp(lbf, rbf, s) + vec3(0, offset1, 0); // bf
		wavyMesh.points[p++] = Lerp(lbn, rbn, s) + vec3(0, offset, 0); // bn
	}
	shift += 0.05;
	shift1 += 0.03;
	SetWavyIndices();
	wavyMesh.Buffer();
}

// Display

void DrawQuad(int i) {
	int4 q = wavyMesh.quads[i];
	Quad(wavyMesh.points[q.i1], wavyMesh.points[q.i2], wavyMesh.points[q.i3], wavyMesh.points[q.i4], false, blu);
}

void DisplayWavyVertices() {
	UseDrawShader(camera.fullview * wavyMesh.transform);
	if (selectedQuad >= 0)
		DrawQuad(selectedQuad);
	else
		for (int i = 0; i < (int)wavyMesh.quads.size(); i++)
			DrawQuad(i);
	for (int i = 0; i < (int)wavyMesh.points.size(); i++)
		Disk(wavyMesh.points[i], 8, yel);
}

void DrawShadowTest() {
	glDisable(GL_DEPTH_TEST);
	int res = 15;
	mat4& m = square.transform;
	vec3 p1 = Xform(m, square.points[0]), p2 = Xform(m, square.points[1]), p3 = Xform(m, square.points[2]), p4 = Xform(m, square.points[3]);
	vec3 offsets[] = { vec3(0, 0, 0), vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, -1, 0), vec3(0, 1, 0), vec3(0, 0, -1), vec3(0, 0, 1) };
	for (int i = 0; i < res; i++)
		for (int j = 0; j < res; j++) {
			float s = (float)i / (res - 1), t = (float)j / (res - 1);
			vec3 p = Lerp(Lerp(p1, p2, s), Lerp(p4, p3, s), t);
			float avg = 0;
			int numLight = 5;
			for (int i = 0; i < numLight; i++) {

				vec3 l = light + 0.1 * normalize(vec3(rand(), rand(), rand()));
				if (IntersectCube(p, l))
					avg++;

			}
			float tmp = (numLight - avg);
			Disk(p, 10, vec3(tmp / (float)numLight / 2 + 0.5, 0, 0));
		}

	mat4& m2 = wall.transform;
	vec3 wp1 = Xform(m2, wall.points[0]), wp2 = Xform(m2, wall.points[1]), wp3 = Xform(m2, wall.points[2]), wp4 = Xform(m2, wall.points[3]);
	for (int i = 0; i < res; i++)
		for (int j = 0; j < res; j++) {
			float s = (float)i / (res - 1), t = (float)j / (res - 1);
			vec3 p = Lerp(Lerp(wp1, wp2, s), Lerp(wp4, wp3, s), t);
			float avg = 0;
			bool hit = IntersectCube(p, light);
			Disk(p, 4, hit ? blu : yel);
		}
}

void DisplayMesh(Meshadow& m) {
	int nTris = m.triangles.size(), nQuads = m.quads.size();
	bool useTexture = m.textureUnit > 0 && m.uvs.size() > 0;
	// enable shader and vertex array object
	int shader = UseMeshadowShader();
	glBindBuffer(GL_ARRAY_BUFFER, m.pos.buffer);
	VertexAttribPointer(shader, "point", 4, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, m.nrm.buffer);
	VertexAttribPointer(shader, "normal", 4, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, m.uv.buffer);
	VertexAttribPointer(shader, "uv", 4, 0, 0);
	// texture
	SetUniform(shader, "useTexture", useTexture);
	if (useTexture) {
		glActiveTexture(GL_TEXTURE0 + m.textureName);   // Unit? active texture corresponds with textureUnit or textureName?
		glBindTexture(GL_TEXTURE_2D, m.textureName);    // bound texture and shader id correspond with textureName
		SetUniform(shader, "textureName", (int)m.textureName);
	}
	// set custom transform and draw (xform = mesh transforms X view transform)
	SetUniform(shader, "modelview", camera.modelview * m.transform);
	SetUniform(shader, "persp", camera.persp);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.eid.buffer);
	glDrawElements(GL_TRIANGLES, 3 * nTris, GL_UNSIGNED_INT, 0); // triangles.data());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDrawElements(GL_QUADS, 4 * nQuads, GL_UNSIGNED_INT, m.quads.data());
	glBindVertexArray(0);
}



void Display(GLFWwindow* w) {
	glClearColor(.5f, .5f, .5f, 1);						// set background color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// clear background and z-buffer
	glEnable(GL_DEPTH_TEST);							// see only nearest surface
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	// display cube object with shadow 
	GLuint s = UseMeshadowShader();				// object use meshadow shader
	SetUniform(s, "facetedShading", !faceted);
	SetUniform(s, "light", vec3(camera.modelview * vec4(light, 1)));
	SetUniform(s, "dim", dim);
	SetUniform(s, "shadowing", true);
	SetUniform(s, "nObjTriangles", (int)object.triangles.size());
	SetUniform(s, "objTransform", camera.modelview * object.transform);
	SetUniform(s, "numlight", numlight);
	SetUniform(s, "lsize", lightRadius);

	//  shader for wavyMesh
	GLuint m = UseMeshShader();					// wavy cube use mesh shader
	SetUniform(m, "facetedShading", faceted);

	// toggle to display Wavy mesh or cube object
	if (flag)
		wavyMesh.Display(camera);
	else
		DisplayMesh(object);

	object.transform = object.transform * RotateX(rot * 0.5) * RotateY(rot * 0.5) * RotateZ(rot * 0.5);		// rotate object
	wavyMesh.transform = Scale(1.0, 1.0, 1.0) * Translate(objectPos);										// position for wavy mesh

	DisplayMesh(square);	// display floor mesh
	DisplayMesh(wall);		// display wall mesh

	UseDrawShader(camera.fullview);
	glEnable(GL_DEPTH_TEST);
	Line(vec3(-2, light.y, light.z), vec3(2, light.y, light.z), 2, red, 0.5);	// sliders for three coordinates
	Line(vec3(light.x, 0, light.z), vec3(light.x, 4, light.z), 2, grn, 0.5);
	Line(vec3(light.x, light.y, -2), vec3(light.x, light.y, 2), 2, blu, 0.5);
	Disk(light, lightRadius*100, wht);
	glDisable(GL_DEPTH_TEST);
	Disk(objectPos, 25, grn); // center point to move object
	glEnable(GL_DEPTH_TEST);
	camera.arcball.Draw();
	for (Slider* slider : sliders)
		slider->Draw(NULL, &slider->color);

	// cpu diagnostics
	if (cpuShadow) DrawShadowTest();			
	if (diagnostics) DisplayWavyVertices();
	

	glFlush();	// finish
}

// Mouse Handlers

void MouseWheel(GLFWwindow* w, double ignore, double spin) {
	camera.MouseWheel(spin);
}

void MouseButton(GLFWwindow* w, int butn, int action, int mods) {
	double x, y;
	mouseEvent = clock();
	glfwGetCursorPos(w, &x, &y);
	y = winHeight - y;
	if (action == GLFW_RELEASE)
		camera.MouseUp();
	picked = NULL;
	if (action == GLFW_PRESS) {
		for (int i = 0; i < sizeof(sliders) / sizeof(Slider*); i++) {
			if (sliders[i]->Hit((int)x, (int)y) && glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
				picked = sliders[i];
				sliders[i]->Mouse((float)x, (float)y);				
				if (picked == &lightdim)
					dim = lightdim.Value();
				
				else if (picked == &freqControl) {
					freq = freqControl.Value();
					MakeWavyPoints();
					SetWavyIndices();
				}

				else if (picked == &lightSize)
					lightRadius = lightSize.Value();
				
				else
					light = vec3(lightx.Value(), lighty.Value(), lightz.Value());
			}
		}
		if (!picked) {
			if (MouseOver(x, y, light, camera.fullview)) {
				picked = &light;
				mover.Down(&light, (int)x, (int)y, camera.modelview, camera.persp);
			}

			else if (MouseOver(x, y, GetBase(object), camera.fullview)) {
				objectPos = GetBase(object);
				picked = &objectPos;
				mover.Down(&objectPos, (int)x, (int)y, camera.modelview, camera.persp);
			}

			else {
				picked = &camera;
				camera.MouseDown(x, y, Shift());
			}
		}
	}
}

void MouseMove(GLFWwindow* w, double x, double y) {
	mouseEvent = clock();
	y = winHeight - y;
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		if (picked == &light)
			mover.Drag((int)x, (int)y, camera.modelview, camera.persp);

		else if (picked == &objectPos) {
			mover.Drag((int)x, (int)y, camera.modelview, camera.persp);
			SetMatrixOrigin(object.transform, objectPos);
		}

		if (picked == &camera)
			camera.MouseDrag(x, y);
		else {
			for (Slider* slider : sliders) {
				if (picked == slider) {
					((Slider*)picked)->Mouse((float)x, (float)y);
					if (picked == &lightdim)
						dim = lightdim.Value();

					else if (picked == &freqControl) {
						freq = freqControl.Value();
						MakeWavyPoints();
						SetWavyIndices();
					}

					else if (picked == &lightSize) 
						lightRadius = lightSize.Value();
					
					else
						light = vec3(lightx.Value(), lighty.Value(), lightz.Value());
				}
			}
		}
	}
}

void KeyBoard(GLFWwindow* window, int key, int scancode, int action, int mods) {
	bool shift = mods & GLFW_MOD_SHIFT;
	int nquads = wavyMesh.quads.size();
	if (action != GLFW_RELEASE) {
		if (key == GLFW_KEY_F) {
			faceted = !faceted;
			MakeWavyPoints();
		}

		else if (key == GLFW_KEY_D) {
			diagnostics = !diagnostics;
		}

		else if (key == GLFW_KEY_L && currentTexture <= objTextureEndIndex) {
			object.textureName = object.textureName++;
			currentTexture++;
		}

		else if (key == GLFW_KEY_K && currentTexture > objTextureStartIndex) {
			object.textureName = object.textureName--;
			currentTexture--;
		}

		else if (key == GLFW_KEY_S)
			cpuShadow = !cpuShadow;

		else if (key == GLFW_KEY_Q && rot > 0)
			rot--;

		else if (key == GLFW_KEY_E)
			rot++;

		else if (key == GLFW_KEY_R) {
			light = lightDefaultPos;
			objectPos = objDefaultPos;
			rot = 0;
			numlight = 1;
			SetMatrixOrigin(object.transform, objectPos);
			object.transform = Translate(objectPos);
		}

		else if (key == GLFW_KEY_EQUAL)
			numlight++;

		else if (key == GLFW_KEY_MINUS && numlight > 1)
			numlight--;

		else if (key == GLFW_KEY_Q) {
			selectedQuad += shift ? -1 : 1;
			selectedQuad = selectedQuad < 0 ? -1 : selectedQuad >= nquads ? nquads - 1 : selectedQuad;
		}

		else if (key == GLFW_KEY_1) {
			ampl *= shift ? .9f : 1.1f;
			MakeWavyPoints();
		}

		else if (key == GLFW_KEY_2) {
			freq *= shift ? .9f : 1.1f;
			MakeWavyPoints();
		}

		else if (key == GLFW_KEY_0)
			flag = !flag;

		else if (key == GLFW_KEY_3) {
			(shift && res > 2) ? res-- : res++;
			MakeWavyPoints();
		}
	}
}

// Application

void Resize(GLFWwindow* window, int width, int height) {
	camera.Resize(winWidth = width, winHeight = height);
	lightx.y = (winHeight - 20);
	lighty.y = (winHeight - 40);
	lightz.y = (winHeight - 60);
	lightdim.y = (winHeight - 80);
	lightSize.y = (winHeight - 100);
	glViewport(0, 0, width, height);
}

int main(int ac, char** av) {
	// init app window, GL context, mesh
	glfwInit();
	GLFWwindow* w = glfwCreateWindow(winWidth, winHeight, "Group-6 3D", NULL, NULL);
	glfwSetWindowPos(w, 100, 100);
	glfwMakeContextCurrent(w);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	printf("Usage:%s", usage);					// keyboard usage
	MakeWavyPoints();							// wavycube point


	// set up for wall and ground
	square.Read(squareFile, squareTexFile, 1, NULL, true, 0);
	square.transform = Scale(2, 2, 2) * Translate(0, -.1, -.01f);
	wall.Read(squareFile, squareTexFile, 1, NULL, true, 0);
	wall.transform = Translate(0, 1, -2.5) * Scale(2, 2, 2) * RotateX(90);

	// set up a list of ready textures for the cube to use
	textureNames.push_back(cubeTexFile);
	textureNames.push_back(cubeTexFile1);
	textureNames.push_back(cubeTexFile2);
	textureNames.push_back(cubeTexFile3);

	// *** use binding offset of 20 for object vertices, 23 for object eids (triangles)
	if (useCube) {
		object.Read(cubeFile, textureNames, 1, NULL, true, 12);
		object.transform = Scale(1.0, 1.0, 1.0) * Translate(objectPos);
	}
	else {
		object.Read(catFile, catTexFile, 1, NULL, true, 12);
		printf("%i vertices, %i triangles\n", object.points.size(), object.triangles.size());
		object.transform = Translate(0, .7f, 0);
	}

	// callbacks
	glfwSetCursorPosCallback(w, MouseMove);
	glfwSetMouseButtonCallback(w, MouseButton);
	glfwSetScrollCallback(w, MouseWheel);
	glfwSetKeyCallback(w, KeyBoard);
	glfwSetWindowSizeCallback(w, Resize);


	// event loop
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(w)) {
		MakeWavyPoints();
		Display(w);
		glfwPollEvents();
		glfwSwapBuffers(w);
	}
	glfwDestroyWindow(w);
	glfwTerminate();
}