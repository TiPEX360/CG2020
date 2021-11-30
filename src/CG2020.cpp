#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <glm/glm.hpp>
#include <CanvasPoint.h>
#include <Colour.h>
#include <CanvasTriangle.h>
#include <TextureMap.h>
#include <time.h>
#include "ModelTriangle.h"
#include "math.h"
#include "Material.h"
#include "RayTriangleIntersection.h"
#include <glm/gtx/string_cast.hpp>
#include "KDTree.h"

#define WIDTH 800
#define HEIGHT 600

std::vector<std::vector<float>> ZBuffer;
std::vector<std::pair<ModelTriangle, Material>> pairs;
bool orbitMode = false;
bool photonsExist = false;
KDTree PHOTONMAP;
bool photonmode = false;
enum RenderMode { WIREFRAME, RASTERIZING, RAYTRACING };

RenderMode renderMode = RASTERIZING;

class Camera {
	public:
		glm::vec3 pos = glm::vec3(0.0, 0.0, 4.0);
		glm::mat3 rot = glm::mat3(1.0);
		float f = 2.0;
		float speed = 0.05;
		double rSpeed = glm::radians(0.5);
};

Camera camera;

glm::vec3 lightSource;

std::vector<float> interpolateSingleFloats(float from, float to, int numberOfValues) {
	std::vector<float> values(numberOfValues);

	float interval = (to - from) / (numberOfValues - 1);
	for(int i = 0; i < numberOfValues; i++) values[i] = from + i*interval;

	return values;
}

std::vector<glm::vec3> interpolateVector(glm::vec3 from, glm::vec3 to, int numberOfValues) {
	std::vector<glm::vec3> values(numberOfValues);

	glm::vec3 intervals = (to - from) / (float)(numberOfValues - 1);
	for(int i = 0; i < numberOfValues; i++) values[i] = from + (float)i*intervals;
	return values;
}

std::vector<glm::vec2> interpolateVector(glm::vec2 from, glm::vec2 to, int numberOfValues) {
	std::vector<glm::vec2> values(numberOfValues);

	glm::vec2 intervals = (to - from) / (float)(numberOfValues - 1);
	for(int i = 0; i < numberOfValues; i++) values[i] = from + (float)i*intervals;

	return values;
}


std::vector<CanvasPoint> interpolateVector(CanvasPoint from, CanvasPoint to, int numberOfValues) {
	std::vector<CanvasPoint> values(numberOfValues);

	glm::vec3 intervals = (glm::vec3(to.x,to.y,to.depth) - glm::vec3(from.x,from.y,from.depth)) / (float)(numberOfValues);
	for(int i = 0; i < numberOfValues; i++) {
		glm::vec3 result = glm::vec3(from.x, from.y, from.depth) + (float)i*intervals;
		if(glm::isnan(result.x)) {
			std::cout << "nan intervals: " << intervals.x << " num: " << numberOfValues << " to.x: "<< to.x << " from.x: " << from.x << std::endl;
			std::cout << "from.y: " << from.y << " to.y: " << to.y << std::endl;
		}
		values[i] = CanvasPoint(result.x,result.y, result.z);
	} 		


	return values;
}

uint32_t colourPack(Colour colour, int alpha) {
	return (alpha << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue;
}

void drawLine(DrawingWindow &window, CanvasPoint from, CanvasPoint to, Colour colour) {
	glm::vec3 src(from.x, from.y, from.depth);
	glm::vec3 dest(to.x, to.y, to.depth);
	glm::vec3 difference = dest - src;
	float numberOfSteps = glm::max(glm::abs(difference.x), glm::abs(difference.y));
	glm::vec3 step = difference/numberOfSteps;	
	for(float i = 0; i < numberOfSteps; i++) {
		glm::vec3 pixel = src + step * i;
		//add pixel to depth buffer if bigger than whats there already
		int X = (int)glm::floor(pixel.x);
		int Y = (int)glm::floor(pixel.y);
		if(X < 0 || X > WIDTH - 1 || Y < 0 || Y > HEIGHT - 1) {
			continue;
		}

	

		if(pixel.z == 0) {
			ZBuffer[X][Y] = pixel.z;
			window.setPixelColour(X, Y, colourPack(colour, 0xFF));
		}
		else if(pixel.z > (ZBuffer[X][Y])) {
			ZBuffer[X][Y] = pixel.z;
			window.setPixelColour(X, Y, colourPack(colour, 0xFF));
		}
	}
}

void drawTriangle(DrawingWindow &window, CanvasTriangle triangle, Colour colour) {
	drawLine(window, triangle.v0(), triangle.v1(), colour);
	drawLine(window, triangle.v1(), triangle.v2(), colour);
	drawLine(window, triangle.v2(), triangle.v0(), colour);
}

std::vector<CanvasPoint> getSortedTriangeVertices(std::vector<CanvasPoint> vertices) {
	if(vertices[0].y > vertices[1].y) std::swap(vertices[0], vertices[1]);
	if(vertices[1].y > vertices[2].y) std::swap(vertices[1], vertices[2]);
	if(vertices[0].y > vertices[1].y) std::swap(vertices[0], vertices[1]);
	return vertices;
}

CanvasPoint getTriangleExtraPoint(CanvasPoint top, CanvasPoint middle, CanvasPoint bottom) {
	float deltaX = bottom.x - top.x;
	float deltaY = bottom.y - top.y;
	
	float gradient = deltaX / deltaY;
	float Y =  middle.y;
	float relY = (middle.y - top.y);
	float X = top.x + relY * gradient;
	
	float depth = INFINITY;
	float distanceTB = glm::length(glm::vec2(deltaX,deltaY));
	float distanceTE = glm::length(glm::vec2((X - top.x),(Y - top.y)));
	float zDeltaTB = bottom.depth - top.depth;
	float zDeltaTE = (zDeltaTB*distanceTE)/distanceTB ;
	depth = zDeltaTE + top.depth;
	
	return CanvasPoint(X,Y,depth);
}


std::vector<CanvasPoint> rasterizeTriangle(CanvasTriangle triangle) {
	std::vector<CanvasPoint> vertices;
	vertices.resize(3);
	for(int i = 0; i < 3; i++) vertices[i] = triangle.vertices[i];

	//Sorting CanvasPoints
	std::vector<CanvasPoint> sorted = getSortedTriangeVertices(vertices);

	CanvasPoint top = sorted[0];
	CanvasPoint middle = sorted[1];
	CanvasPoint bottom = sorted[2];
	//Finding extra point
	CanvasPoint extra = getTriangleExtraPoint(top,middle,bottom);
	std::vector<CanvasPoint> result;
	result.resize(4);
	result[0] = top;
	result[1] = middle;
	result[2] = bottom;
	result[3] = extra;

	return result;
}

std::vector<CanvasPoint> rasterizeTriangle(std::vector<CanvasPoint> vertices) {
	//Assume the vertices are already sorted
	//Finding extra point
	CanvasPoint extra = getTriangleExtraPoint(vertices[0],vertices[1],vertices[2]); //top mid bottom
	std::vector<CanvasPoint> result;
	result.resize(4);
	result[0] = vertices[0];//top
	result[1] = vertices[1]; //mid
	result[2] = vertices[2]; //bottom
	result[3] = extra;

	return result;
}

void fillTriangle(DrawingWindow &window, CanvasTriangle triangle, Colour colour) {
	std::vector<CanvasPoint> vertices = rasterizeTriangle(triangle);
	CanvasPoint top = vertices[0];
	CanvasPoint middle = vertices[1];
	CanvasPoint bottom = vertices[2];
	CanvasPoint extra = vertices[3];
	
	//side to side interpolation
	std::vector<CanvasPoint> topToMiddle = interpolateVector(top, middle, (int)(middle.y - top.y)+1);
	std::vector<CanvasPoint> topToExtra = interpolateVector(top, extra, (int)(extra.y - top.y)+1);
	std::vector<CanvasPoint> bottomToMiddle = interpolateVector(bottom, middle, (int)(bottom.y - middle.y)+1);
	std::vector<CanvasPoint> bottomToExtra = interpolateVector(bottom, extra, (int)(bottom.y - middle.y)+1);

	//filling top part
	for(int i=0; i < topToMiddle.size(); i++) {
		drawLine(window, topToMiddle[i], topToExtra[i], colour);
	}
	//filling bottom part
	for(int i=0; i < bottomToMiddle.size(); i++) {
		drawLine(window, bottomToMiddle[i], bottomToExtra[i], colour);
	}
}

void drawFilledTriangle(DrawingWindow &window, CanvasTriangle triangle, Colour lineColour, Colour fillColour) {
	fillTriangle(window, triangle, fillColour);
	drawTriangle(window,triangle,lineColour);
}

CanvasPoint calcExtraTexturePoint(std::vector<CanvasPoint> rCanvas, std::vector<CanvasPoint> rTexture) {
	glm::vec3 cTopToExtra = glm::vec3(rCanvas[3].x, rCanvas[3].y, rCanvas[3].depth) - glm::vec3(rCanvas[0].x, rCanvas[0].y, rCanvas[0].depth);
	glm::vec3 cTopToBottom = glm::vec3(rCanvas[2].x, rCanvas[2].y, rCanvas[2].depth) - glm::vec3(rCanvas[0].x, rCanvas[0].y, rCanvas[0].depth);
	glm::vec3 ratio = cTopToExtra / cTopToBottom;
	glm::vec3 tTopToBottom = glm::vec3(rTexture[2].x, rTexture[2].y, rTexture[2].depth) - glm::vec3(rTexture[0].x, rTexture[0].y,  rTexture[0].depth);
	glm::vec3 tTopToExtra = tTopToBottom * ratio;

	//add depth

	return CanvasPoint(rTexture[0].x + tTopToExtra.x, rTexture[0].y + tTopToExtra.y, rTexture[0].depth + tTopToExtra.z);
}


void drawTexturedTriangle(DrawingWindow &window, CanvasTriangle triangle, std::string path) {
	TextureMap texture(path);
	std::vector<CanvasPoint> initCanvasPoints({triangle.v0(),triangle.v1(),triangle.v2()});
	std::vector<CanvasPoint> sortedCanvasPoints = getSortedTriangeVertices(initCanvasPoints);
	
	CanvasPoint tTop(sortedCanvasPoints[0].texturePoint.x, sortedCanvasPoints[0].texturePoint.y);
	CanvasPoint tMiddle(sortedCanvasPoints[1].texturePoint.x, sortedCanvasPoints[1].texturePoint.y);
	CanvasPoint tBottom(sortedCanvasPoints[2].texturePoint.x, sortedCanvasPoints[2].texturePoint.y);

	std::vector<CanvasPoint> rCanvas = rasterizeTriangle(sortedCanvasPoints);
	std::vector<CanvasPoint> rTexture({tTop, tMiddle, tBottom});

	//Getting extra for texture
	CanvasPoint tExtra = calcExtraTexturePoint(rCanvas, rTexture);
	if(glm::isnan(rCanvas[1].x)) std::cout << "bad" << std::endl;
	//Interpolate canvas
	std::vector<CanvasPoint> cTopToMiddleI = interpolateVector(rCanvas[0], rCanvas[1], (int)(rCanvas[1].y- rCanvas[0].y)+1);
	std::vector<CanvasPoint> cTopToExtraI = interpolateVector(rCanvas[0], rCanvas[3], (int)(rCanvas[3].y - rCanvas[0].y)+1);
	std::vector<CanvasPoint> cBottomToMiddleI = interpolateVector(rCanvas[2], rCanvas[1], (int)(rCanvas[2].y - rCanvas[1].y)+1);
	std::vector<CanvasPoint> cBottomToExtraI = interpolateVector(rCanvas[2], rCanvas[3], (int)(rCanvas[2].y - rCanvas[3].y)+1);

	//Interpolate texure
	std::vector<CanvasPoint> tTopToMiddleI = interpolateVector(tTop, tMiddle, (int)(rCanvas[1].y- rCanvas[0].y)+1);
	std::vector<CanvasPoint> tTopToExtraI = interpolateVector(tTop, tExtra, (int)(rCanvas[3].y - rCanvas[0].y)+1);
	std::vector<CanvasPoint> tBottomToMiddleI = interpolateVector(tBottom, tMiddle, (int)(rCanvas[2].y- rCanvas[1].y)+1);
	std::vector<CanvasPoint> tBottomToExtraI = interpolateVector(tBottom, tExtra, (int)(rCanvas[2].y - rCanvas[3].y)+1);

	//Sample and paint top triangle
	for(int i = 0; i < tTopToMiddleI.size(); i++) {
		glm::vec3 tSrc(tTopToMiddleI[i].x, tTopToMiddleI[i].y, tTopToMiddleI[i].depth);
		glm::vec3 tDest(tTopToExtraI[i].x, tTopToExtraI[i].y, tTopToExtraI[i].depth);

		glm::vec3 cSrc(cTopToMiddleI[i].x, cTopToMiddleI[i].y, cTopToMiddleI[i].depth);
		glm::vec3 cDest(cTopToExtraI[i].x, cTopToExtraI[i].y, cTopToExtraI[i].depth);
		//if(glm::isnan(cSrc.x)) std::cout << i << std::endl;
		glm::vec3 tDifference = tDest - tSrc;
		glm::vec3 cDifference = cDest - cSrc;
		float numberOfSteps = glm::max(glm::abs(cDifference.x), glm::abs(cDifference.y));
		glm::vec3 tStep = tDifference/numberOfSteps;
		glm::vec3 cStep = cDifference/numberOfSteps;	
		for(float j = 0; j < numberOfSteps; j++) {
			glm::vec3 tPixel = glm::floor(tSrc + tStep * j);
			glm::vec3 cPixel = cSrc + cStep * j;
			uint32_t colour = texture.pixels[(int)tPixel.x + (texture.width) * (int)tPixel.y];
			//add pixel to depth buffer if bigger than whats there already
			int X = (int) glm::floor(cPixel.x);
			int Y = (int) glm::floor(cPixel.y);
			if(X < 0 || X > WIDTH - 1 || Y < 0 || Y > HEIGHT - 1) {
				continue;
			}
			if(cPixel.z == 0) {
				ZBuffer[X][Y] = cPixel.z;
				window.setPixelColour(X, Y, colour);
			}
			else if(cPixel.z > (ZBuffer[X][Y])) {
				ZBuffer[X][Y] = cPixel.z;
				window.setPixelColour(X, Y, colour);
			}
			
		}

	}

	//Sample and paint bottom triangle
	for(int i = 0; i < tBottomToMiddleI.size(); i++) {
		glm::vec3 tSrc(tBottomToMiddleI[i].x, tBottomToMiddleI[i].y, tBottomToMiddleI[i].depth);
		glm::vec3 tDest(tBottomToExtraI[i].x, tBottomToExtraI[i].y, tBottomToExtraI[i].depth);
		
		glm::vec3 cSrc(cBottomToMiddleI[i].x, cBottomToMiddleI[i].y, cBottomToMiddleI[i].depth);
		glm::vec3 cDest(cBottomToExtraI[i].x, cBottomToExtraI[i].y, cBottomToExtraI[i].depth);
		glm::vec3 tDifference = tDest - tSrc;
		glm::vec3 cDifference = cDest - cSrc;
		float numberOfSteps = glm::max(glm::abs(cDifference.x), glm::abs(cDifference.y));
		glm::vec3 tStep = tDifference/numberOfSteps;
		glm::vec3 cStep = cDifference/numberOfSteps;	
		for(float j = 0; j < numberOfSteps; j++) {
			glm::vec3 tPixel = glm::floor(tSrc + tStep * j);
			glm::vec3 cPixel = cSrc + cStep * j;
			uint32_t colour = texture.pixels[(int)tPixel.x + (texture.width) * (int)tPixel.y];
			//add pixel to depth buffer if bigger than whats there already
			int X = (int) glm::floor(cPixel.x);
			int Y = (int) glm::floor(cPixel.y);
			if(X < 0 || X > WIDTH - 1 || Y < 0 || Y > HEIGHT - 1) {
				continue;
			}
			if(cPixel.z == 0) {
				ZBuffer[X][Y] = cPixel.z;
				window.setPixelColour(X, Y, colour);
			}
			else if(cPixel.z > (ZBuffer[X][Y])) {
				ZBuffer[X][Y] = cPixel.z;
				window.setPixelColour(X, Y, colour);
			}
		}
	}	
	
}

//Calculate possible solution of ray tracing intersection
glm::vec3 getPossibleIntersectionSolution(ModelTriangle triangle, glm::vec3 rayOrigin ,glm::vec3 rayDirection) {
	glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
	glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
	glm::vec3 SPVector = rayOrigin - triangle.vertices[0];
	glm::mat3 DEMatrix(-rayDirection, e0, e1);
	glm::vec3 possibleSolution = glm::inverse(DEMatrix) * SPVector;
	return possibleSolution;
}

//Check if the possible solution is valid (inside the triangle)
bool isValidIntersection(glm::vec3 tuvVector) {
	float t = tuvVector[0];
	float u = tuvVector[1];
	float v = tuvVector[2];
	return (u >= 0.0) && (u <= 1.0) && (v >= 0.0) && (v <= 1.0) && (u + v) <= 1.0 && t > 0.0;
}

//Calculate 3D coordinate of the intersection
RayTriangleIntersection getRayTriangleIntersection(ModelTriangle triangle, glm::vec3 validVector) {
	glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
	glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
	float t = validVector[0];
	float u = validVector[1];
	float v = validVector[2];
	glm::vec3 r = triangle.vertices[0] + u*e0 + v*e1;
	RayTriangleIntersection result = RayTriangleIntersection();

	bool found = false;
	for(int i = 0; i < pairs.size() && !found; i++) {
		ModelTriangle tri = pairs[i].first;
		if(tri.vertices[0] == triangle.vertices[0] && tri.vertices[1] == triangle.vertices[1] && tri.vertices[2] == triangle.vertices[2]) {
			result.triangleIndex = i;
			found = true;
		}
	}

	result.intersectionPoint = r;
	result.distanceFromCamera = t;
	result.intersectedTriangle = triangle;
	return result;
}


CanvasTriangle getRandomTriangle() {
	CanvasPoint point0((float)(rand() % WIDTH), (float)(rand() % HEIGHT));
	CanvasPoint point1((float)(rand() % WIDTH), (float)(rand() % HEIGHT));
	CanvasPoint point2((float)(rand() % WIDTH), (float)(rand() % HEIGHT));
	CanvasTriangle triangle(point0, point1, point2);
	
	return triangle;
}

Material loadMaterial(std::string mtlName, std::string mtlPath) {
    std::ifstream in(mtlPath, std::ifstream::in);
	std::string line;
	glm::vec3 colour;
	std::string texturePath;
	
    bool found = false;
    while(!found && !in.eof()) {
	    std::getline(in, line);
        std::vector<std::string> tokens = split(line, ' ');
        if(tokens[0].compare("newmtl") == 0 && tokens[1].compare(mtlName) == 0) {
            found = true;
        }
    }
    found = false;
	while(!in.eof() && !found) {
        std::getline(in, line);
        std::vector<std::string> tokens = split(line, ' ');
        if(tokens[0].compare("Kd") == 0) {
            colour = glm::vec3(0xFF*stof(tokens[1]), 0xFF*stof(tokens[2]), 0xFF*stof(tokens[3]));
        }
		if(tokens[0].compare("map_Kd") == 0) {
			texturePath = tokens[1];
		}
		if(tokens[0].compare("newmtl") == 0) {
			found = true;
		}
    }
	Material material = Material(Colour(colour.r, colour.g, colour.b), texturePath, mtlName);
    in.close();
    return material;
}

std::vector<std::pair<ModelTriangle, Material>> loadObj(std::string path, float scale) {
	std::ifstream file(path, std::ifstream::in);
	std::string line;
	
	std::vector<std::pair<ModelTriangle, Material>> pairs;
	std::vector<glm::vec3> vertices;
	std::vector<TexturePoint> texturePoints;

	Material material;
	std::string mtlPath;


	while(!file.eof()) {
		std::getline(file, line);
		std::vector<std::string> tokens = split(line, ' ');

				std::cout << line << std::endl;
		if(tokens[0].compare("mtllib") == 0) mtlPath = tokens[1];
		else if(tokens[0].compare("usemtl") == 0) {
			material = loadMaterial(tokens[1], mtlPath);
			if(tokens[1].compare("Mirror") == 0) material.mirror = true;
		}
		else if(tokens[0].compare("v") == 0) vertices.push_back(scale * glm::vec3(stof(tokens[1]), stof(tokens[2]), stof(tokens[3])));
		else if(tokens[0].compare("vt") == 0) {
			texturePoints.push_back(TexturePoint(stof(tokens[1]), stof(tokens[2])));
		}
		
		else if(tokens[0].compare("f") == 0) {
			//For each index in f
			std::array<glm::vec3, 3> trianglePoints;
			std::array<TexturePoint, 3> selectedTexturePoints;
			for(int i = 1; i < 4; i++) {
				std::vector<std::string> subTokens = split(tokens[i], '/');
				//TrianglePoint
				trianglePoints[i - 1] = vertices[stoi(subTokens[0]) - 1];
				//TexturePoint
				if(subTokens[1].compare("\0") != 0) {
					selectedTexturePoints[i - 1] = texturePoints[stoi(subTokens[1]) - 1];
				}
			}

			ModelTriangle triangle = ModelTriangle();
			triangle.vertices = trianglePoints;
			triangle.texturePoints = selectedTexturePoints;
			triangle.colour = material.colour;
			glm::vec3 e0 = triangle.vertices[1] - triangle.vertices[0];
			glm::vec3 e1 = triangle.vertices[2] - triangle.vertices[0];
			triangle.normal = glm::normalize(glm::cross(e0, e1));
			
			pairs.push_back(std::pair<ModelTriangle,Material>(triangle,material));
		}
	}
	
	file.close();
	return pairs;
}

void drawModelTriangle(DrawingWindow &window, std::pair<ModelTriangle, Material> pair) {
	ModelTriangle triangle = pair.first;
	Material material = pair.second;

	std::vector<glm::vec3> renderPos;
	for(int i=0; i < triangle.vertices.size(); i++) {
		glm::vec3 vertex = triangle.vertices[i] - camera.pos;
		vertex = camera.rot * vertex;
		float u = glm::floor(-1*camera.f*(vertex.x / vertex.z)*(HEIGHT*1.5)+ WIDTH/2);
		float v = glm::floor(camera.f*(vertex.y / vertex.z)*(HEIGHT*1.5) + HEIGHT/2);
		float Z = INFINITY;
		
		if(vertex.z != 0.0) {
			Z = glm::abs(1 / vertex.z);
		}
		renderPos.push_back(glm::vec3(u, v, Z));
	}
	CanvasTriangle transposedTri = CanvasTriangle(CanvasPoint(renderPos[0].x, renderPos[0].y, renderPos[0].z), CanvasPoint(renderPos[1].x, renderPos[1].y, renderPos[1].z) ,CanvasPoint(renderPos[2].x, renderPos[2].y, renderPos[2].z));

	if(material.texturePath.empty()) {
		drawFilledTriangle(window, transposedTri, triangle.colour, triangle.colour);

	} else {		
		TextureMap textureMap(material.texturePath);

		transposedTri.v0().texturePoint = TexturePoint(triangle.texturePoints[0].x * textureMap.width, textureMap.height - triangle.texturePoints[0].y * textureMap.height);
		transposedTri.v1().texturePoint = TexturePoint(triangle.texturePoints[1].x * textureMap.width, textureMap.height - triangle.texturePoints[1].y * textureMap.height);
		transposedTri.v2().texturePoint = TexturePoint(triangle.texturePoints[2].x * textureMap.width, textureMap.height - triangle.texturePoints[2].y * textureMap.height);
		drawTexturedTriangle(window, transposedTri, material.texturePath);
	}
}


// void lookAt() {
// 	glm::vec3 forward = glm::normalize(camera.pos);
// 	glm::vec3 right = -glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
// 	glm::vec3 up = -glm::normalize(glm::cross(forward, -right));
// 	camera.rot = glm::transpose(glm::mat3(right, up, forward));
// }

glm::mat3 lookAt() {
	glm::vec3 forward = glm::normalize(camera.pos);
	glm::vec3 right = -glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
	glm::vec3 up = -glm::normalize(glm::cross(forward, -right));
	return glm::transpose(glm::mat3(right, up, forward));
}

glm::mat3 lookAt(glm::vec3 target) {
	glm::vec3 forward = glm::normalize(camera.pos - target);
	glm::vec3 right = -glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
	glm::vec3 up = -glm::normalize(glm::cross(forward, -right));
	return glm::transpose(glm::mat3(right, up, forward));
}

std::vector<glm::vec3> calcVertexNormals(ModelTriangle triangle) {
	std::vector<glm::vec3> normals;
	for(int v = 0; v < 3; v++) {
		std::vector<ModelTriangle> neighbours;
		ModelTriangle comparison;
		//Get neighbours
		for(int i = 0; i < pairs.size(); i++) {
			comparison = pairs[i].first;
			if(triangle.vertices[v] == comparison.vertices[0] || triangle.vertices[v] == comparison.vertices[1] || triangle.vertices[v] == comparison.vertices[2]) {
				neighbours.push_back(comparison);
			}
		}
		//Calc normal from neighbours
		glm::vec3 normal = glm::vec3(0);
		for(int n = 0; n < neighbours.size(); n++) {
			normal = normal + neighbours[n].normal;
		}
		normal /= neighbours.size();
		normals.push_back(glm::normalize(normal));
	}

	return normals;
}
float gaussian(float x, float m, float s) {
	return (( 1 / ( s * sqrt(2*M_PI) ) ) * exp( -0.5 * pow( (x-m)/s, 2.0 )));
}

void rayTracing(DrawingWindow &window, std::vector<std::pair<ModelTriangle,Material>> pairs, float scale) {
	//For each pixel on screen
	for(int u = 0; u < WIDTH; u++) {
		for(int v = 0; v < HEIGHT; v++) {
			//Calc ray direction
			glm::vec3 cameraSpaceCanvasPixel((u - WIDTH/2), (HEIGHT/2 - v), -camera.f*WIDTH);
			glm::vec3 worldSpaceCanvasPixel = (cameraSpaceCanvasPixel * camera.rot) + camera.pos;
			glm::vec3 rayDirection = glm::normalize(worldSpaceCanvasPixel - camera.pos);
			std::vector<RayTriangleIntersection> intersections;
			std::vector<Material> materials;
			//Check if ray intersects any of tri planes
			for(int i = 0; i < pairs.size(); i++) {
				ModelTriangle triangle = pairs[i].first;
				Material material = pairs[i].second;
				//Check if intersects current triangle pace
				glm::vec3 tuvVector = getPossibleIntersectionSolution(triangle, camera.pos, rayDirection);
				//Is the intersection in the triangle
				if(isValidIntersection(tuvVector)) {
					RayTriangleIntersection intersection = getRayTriangleIntersection(triangle, tuvVector);
					//Add intersection to vector
					intersections.push_back(intersection);
					materials.push_back(material);
				}
			}

			if(!intersections.empty()) {
				//Get closest intersection
				RayTriangleIntersection closest = intersections[0];
				Material closestMat = materials[0];
				for(int i = 0; i < intersections.size(); i++) {
					if(intersections[i].distanceFromCamera <= closest.distanceFromCamera){
						closest = intersections[i];
						closestMat = materials[i];
					} 
				}

				//Bounce mirror rays
				bool sky = false;
				RayTriangleIntersection prevInt = closest;
				Material prevMat = closestMat;
				if(closestMat.mirror) {
					glm::vec3 rSrc = closest.intersectionPoint;
					glm::vec3 rDir = glm::normalize(rSrc - camera.pos) - 2.0f*closest.intersectedTriangle.normal*glm::dot(glm::normalize(rSrc - camera.pos), closest.intersectedTriangle.normal);
					std::vector<RayTriangleIntersection> ints;
					std::vector<Material> mats;
					RayTriangleIntersection cInt = closest;
					Material cMat = closestMat;

					for(int i = 0; i < pairs.size(); i++) {
						ModelTriangle t = pairs[i].first;
						Material m = pairs[i].second;
						glm::vec3 tuv = getPossibleIntersectionSolution(t, rSrc, rDir);
						if(isValidIntersection(tuv)) {
							RayTriangleIntersection inte = getRayTriangleIntersection(t, tuv);
							//Add intersection to vector
							if(tuv[0] > 0.001f){
								ints.push_back(inte);
								mats.push_back(m);
							}
						}
					}

					if(!ints.empty()) {
						cInt = ints[0];
						cMat = materials[0];
						for(int i = 0; i < ints.size(); i++) {
							if(glm::distance(ints[i].intersectionPoint, rSrc) <= glm::distance(cInt.intersectionPoint, rSrc)){
								cInt = ints[i];
								cMat = mats[i];
							} 
						}	
					} 
					else {
						
						sky = true;
					}
					std::cout << "intersectionc" << std::endl;
					closest = cInt;
					closestMat = cMat;
				}

				//Get photon
				std::vector<Node*> photons = PHOTONMAP.beginSearch(closest.intersectionPoint);
				float intensity = 0;
				int n = 0;
				float factor = 0;
				// int n = (int)photons.size();
				for(int i = 0; i < photons.size(); i++) {
					float d = glm::distance(closest.intersectionPoint, photons[i]->loc);
					if(d <= 0.05f){
						// intensity += photons[i]->intensity*glm::exp(-d);
						intensity += photons[i]->intensity*gaussian(d, 0.0f, 0.4f);
						factor+=gaussian(d, 0.0f, 0.4f);
						n++;
					}
				}
				intensity /= factor;

				//Phong shading
				std::vector<glm::vec3> vertexNormals = calcVertexNormals(closest.intersectedTriangle);
				glm::vec3 tuv = getPossibleIntersectionSolution(closest.intersectedTriangle, camera.pos, rayDirection);
				// glm::vec3 tuv;
				// if(!prevMat.mirror) tuv = getPossibleIntersectionSolution(closest.intersectedTriangle, camera.pos, rayDirection);
				// else tuv = getPossibleIntersectionSolution(closest.intersectedTriangle, prevInt.intersectionPoint, glm::normalize(closest.intersectionPoint - prevInt.intersectionPoint));
				float v2Factor = tuv[2];
				float v1Factor = tuv[1];
				float v0Factor = 1.0f - v2Factor - v1Factor;
				glm::vec3 phongNormal = glm::normalize((v0Factor * vertexNormals[0] + v1Factor * vertexNormals[1] + v2Factor * vertexNormals[2]));
				
				//?? shading
				// int res = 1000;
				// std::vector<glm::vec3> e01normals = interpolateVector(vertexNormals[0], vertexNormals[1], res);
				// std::vector<glm::vec3> e02normals = interpolateVector(vertexNormals[0], vertexNormals[2], res);
				// std::vector<glm::vec3> e12normals = interpolateVector(vertexNormals[1], vertexNormals[2], res);
				// int distance = glm::round((tuv[1] + tuv[2])*res);
				// std::vector<glm::vec3> internormals = interpolateVector(e01normals[distance], e02normals[distance], res);
				// glm::vec3 phongNormal = internormals[glm::round(tuv[2]*res)];

				//Flat shading
				glm::vec3 flatNormal = closest.intersectedTriangle.normal;
				glm::vec3 normal = flatNormal;
				if(closestMat.name == "Sphere") {
					normal = phongNormal;
				}
				

				std::cout << closestMat.mirror << std::endl;
				
				// std::cout << "Intensity" << intensity << "Distance" << glm::distance(photons[photons.size() - 1]->loc, closest.intersectionPoint) << std::endl;
				//Cast shadow ray
				glm::vec3 shadowRayDirection = glm::normalize(lightSource - closest.intersectionPoint);
				glm::vec3 shadowRayTuv;

				bool shadow = false;
				for(int i = 0; i < pairs.size() && !shadow && !sky; i++) {
					if(closest.triangleIndex != i) {
						shadowRayTuv = getPossibleIntersectionSolution(pairs[i].first, closest.intersectionPoint, shadowRayDirection);
						if(isValidIntersection(shadowRayTuv)) {
							RayTriangleIntersection shadowIntersect = getRayTriangleIntersection(pairs[i].first, shadowRayTuv);	

							if(shadowIntersect.distanceFromCamera < glm::distance(lightSource, closest.intersectionPoint)) {
								shadow = true;
							}else {
								// glm::vec3 normal = closest.intersectedTriangle.normal;
								glm::vec3 facing = glm::normalize(camera.pos - closest.intersectionPoint);
								float angle = glm::acos(glm::dot(facing, normal));
								if(angle > M_PI / 2) shadow = true;
							}
						}
					}
				}

				//Paint to screen
				if(sky) {
					window.setPixelColour(u,v, colourPack(Colour(0.0f, 0.0f, 0.0f), 0xFF));
				  
				}
				else if(!shadow) {
			
					glm::vec3 lightDirection = glm::normalize(lightSource - closest.intersectionPoint);
					glm::vec3 cameraDirection = glm::normalize(camera.pos - closest.intersectionPoint);

					//Specular 
					glm::vec3 rReflection = -lightDirection - 2.0f*normal*glm::dot(-lightDirection, normal);
					float specular = 255.0f*glm::pow(glm::dot(rReflection, cameraDirection), 60);
					// std::cout << specular << std::endl;

					//Incidence Lighting
					float angle = glm::acos(glm::dot(normal, lightDirection)); //radians
					float incidence;
					if(angle > M_PI / 2) {
						incidence = 0;
					} else {
						incidence = 1.0f - 2*angle/M_PI;
					}
					//Light falloff
					float r = glm::distance(lightSource, closest.intersectionPoint);
					float falloff = 1.0f/(4*M_PI*r*r);

					glm::vec3 colour;
					if(photonmode) colour = intensity * glm::vec3(closestMat.colour.red, closestMat.colour.green, closestMat.colour.blue);
					else colour = glm::clamp(specular + 5.0f*glm::clamp(falloff* incidence, 0.1f, 1.0f) * glm::vec3(closestMat.colour.red, closestMat.colour.green, closestMat.colour.blue), 0.0f, 255.0f);
		
					// glm::vec3 colour = 255.0f * glm::abs(pixelNormal);
					window.setPixelColour(u,v,colourPack(Colour(colour.r, colour.g, colour.b), 0xFF));
				} else {
					glm::vec3 colour;
					if(photonmode) colour = intensity * glm::vec3(closestMat.colour.red, closestMat.colour.green, closestMat.colour.blue);
					else colour = 0.2f * glm::vec3(closestMat.colour.red, closestMat.colour.green, closestMat.colour.blue);
					
					// glm::vec3 colour = intensity*glm::vec3(0xFF);
				
					window.setPixelColour(u,v,colourPack(Colour(colour.r, colour.g, colour.b), 0xFF));
				}
			}
		}

	}
}


void handleEvent(SDL_Event event, DrawingWindow &window) {
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_LEFT) {
			camera.pos = glm::vec3(camera.pos.x - camera.speed, camera.pos.y, camera.pos.z);
		}
		else if (event.key.keysym.sym == SDLK_RIGHT) {
			camera.pos = glm::vec3(camera.pos.x + camera.speed, camera.pos.y, camera.pos.z);
		}
		else if (event.key.keysym.sym == SDLK_UP) {
			camera.pos = glm::vec3(camera.pos.x, camera.pos.y, camera.pos.z - camera.speed);
		}
		else if (event.key.keysym.sym == SDLK_DOWN) {
			camera.pos = glm::vec3(camera.pos.x, camera.pos.y, camera.pos.z + camera.speed);
		}
		else if (event.key.keysym.sym == SDLK_LSHIFT) {
			camera.pos = glm::vec3(camera.pos.x, camera.pos.y  + camera.speed, camera.pos.z);
		}
		else if (event.key.keysym.sym == SDLK_LCTRL) {
			camera.pos = glm::vec3(camera.pos.x, camera.pos.y  - camera.speed, camera.pos.z);
		}
		else if (event.key.keysym.sym == SDLK_w) {
			glm::mat3 rotationMat(
									1.0,0.0,0.0,
									0.0,glm::cos(-camera.rSpeed),glm::sin(-camera.rSpeed),
									0.0, -glm::sin(-camera.rSpeed),glm::cos(-camera.rSpeed) 
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_s) {
			glm::mat3 rotationMat(
									1.0,0.0,0.0,
									0.0,glm::cos(camera.rSpeed),glm::sin(camera.rSpeed),
									0.0, -glm::sin(camera.rSpeed),glm::cos(camera.rSpeed) 
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_a) {
			glm::mat3 rotationMat(
									glm::cos(-camera.rSpeed),0.0,-glm::sin(-camera.rSpeed),
									0.0,1.0,0.0,
									glm::sin(-camera.rSpeed),0.0,glm::cos(-camera.rSpeed) 
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_d) {

			glm::mat3 rotationMat(
									glm::cos(camera.rSpeed),0.0,-glm::sin(camera.rSpeed),
									0.0,1.0,0.0,
									glm::sin(camera.rSpeed),0.0,glm::cos(camera.rSpeed) 
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_q) {
			glm::mat3 rotationMat(
									glm::cos(-camera.rSpeed),glm::sin(-camera.rSpeed),0.0,
									-glm::sin(-camera.rSpeed),glm::cos(-camera.rSpeed),0.0,
									0.0,0.0,1.0
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_e) {
			glm::mat3 rotationMat(
									glm::cos(camera.rSpeed),glm::sin(camera.rSpeed),0.0,
									-glm::sin(camera.rSpeed),glm::cos(camera.rSpeed),0.0,
									0.0,0.0,1.0
			);
			camera.rot = rotationMat * camera.rot;
		}
		else if (event.key.keysym.sym == SDLK_SPACE) {
			std::cout << "look at center" << std::endl;
			camera.rot = lookAt();
		}
		else if (event.key.keysym.sym == SDLK_l) {
			std::cout << "Look at lightsource" << std::endl;
			camera.rot = lookAt(lightSource);
		}
		else if (event.key.keysym.sym == SDLK_u) {
			std::cout << "u" << std::endl;
			drawTriangle(window,getRandomTriangle(),Colour(rand() % 255, rand() % 255, rand() % 255));
		}
		else if (event.key.keysym.sym == SDLK_f) {
			std::cout << "f" << std::endl;
			drawFilledTriangle(window,getRandomTriangle(),Colour(0xFF,0xFF,0xFF),Colour(rand() % 255, rand() % 255, rand() % 255));
		}
		else if (event.key.keysym.sym == SDLK_g) {
			std::cout << "g" << std::endl;
			lightSource.y -= 0.1;
		}
		else if (event.key.keysym.sym == SDLK_b) {
			std::cout << "b" << std::endl;
			lightSource.y += 0.1;
		}
		else if(event.key.keysym.sym == SDLK_t) {
			std::cout << "t" << std::endl;
			CanvasPoint p0(160.0,10.0);
			CanvasPoint p1(300.0,230.0);
			CanvasPoint p2(10.0,150.0);

			p0.texturePoint = TexturePoint(195.0, 5.0);
			p1.texturePoint = TexturePoint(396.0, 380.0);
			p2.texturePoint = TexturePoint(65.0, 330.0);
			
			CanvasTriangle textureTriangle(p0,p1,p2);
			drawTexturedTriangle(window, textureTriangle, "texture.ppm");
			drawTriangle(window, textureTriangle,Colour(0xFF,0xFF,0xFF));
		}
		else if(event.key.keysym.sym == SDLK_m) {
			std::cout << "Rasterizing" << std::endl;
			renderMode = RASTERIZING;
		}
		else if(event.key.keysym.sym == SDLK_o) {
			orbitMode = !orbitMode;
		}
		else if(event.key.keysym.sym == SDLK_r) {
			std::cout << "RayTracing" << std::endl;
			renderMode = RAYTRACING;
		}
		else if(event.key.keysym.sym == SDLK_n) {
			std::cout << "Wireframe" << std::endl;
			renderMode = WIREFRAME;
		}
		else if(event.key.keysym.sym == SDLK_z) {
			std::cout << "photons" << std::endl;
			photonmode = !photonmode;
		}
	} else if (event.type == SDL_MOUSEBUTTONDOWN) window.savePPM("output.ppm");
}

KDTree photonMap(std::vector<std::pair<ModelTriangle, Material>> pairs, int amount) {
	std::cout << "building photon map" << std::endl;
	std::vector<glm::vec4> photons;
	for(int p = 0; p < amount; p++) {
		glm::vec3 pDirection = glm::normalize(glm::vec3(rand()%1000 - 500, rand()%1000 - 500, rand()%1000 - 500));
		glm::vec3 pOrigin = lightSource;
		std::vector<RayTriangleIntersection> intersections;
		float intensity = 1.0f;
		bool dead = false;
		// std::cout << "new" << std::endl;
		while(!dead) {
			intersections.clear();
			for(int i = 0; i < pairs.size(); i++) {
					ModelTriangle triangle = pairs[i].first;
					Material material = pairs[i].second;
					glm::vec3 tuvVector = getPossibleIntersectionSolution(triangle, pOrigin, pDirection);
					if(isValidIntersection(tuvVector)) {
						RayTriangleIntersection intersection = getRayTriangleIntersection(triangle, tuvVector);
						//Add intersection to vector
						intersections.push_back(intersection);
					}
			}
			// std::cout << intensity << std::endl;
			if(intersections.empty()) dead = true;
			else{
				
				RayTriangleIntersection closest = intersections[0];
				for(int i = 1; i < intersections.size(); i++) {
					if(glm::distance(intersections[i].intersectionPoint, pOrigin) <= glm::distance(closest.intersectionPoint, pOrigin)){
						closest = intersections[i];
					} 
				}
				
				// std::cout << intensity << std::endl;
				photons.push_back(glm::vec4(closest.intersectionPoint, intensity));
				
				// intensity *= 0.8*glm::length(glm::vec3(closestMat.colour.red, closestMat.colour.green, closestMat.colour.blue))/glm::length(glm::vec3(255.0f, 255.0f, 255.0f)); //check this val correct
				intensity *= 0.4;
				if(rand()%100 < 50) dead = true;
				else {
					glm::vec3 rReflection = pDirection - 2.0f*closest.intersectedTriangle.normal*glm::dot(pDirection, closest.intersectedTriangle.normal);
					// float theta = (rand() % 100)*M_PI/400;
					// glm::mat3 xRot = {1, 0, 0, 0, cos(theta), -sin(theta), 0, sin(theta), cos(theta)};
					// glm::mat3 yRot = {cos(theta), 0, sin(theta), 0, 1, 0, -sin(theta), 0, cos(theta)};
					// glm::mat3 zRot = {cos(theta), -sin(theta), 0, sin(theta), cos(theta), 0, 0, 0, 1};
					// // pDirection = zRot * yRot * xRot * rReflection;
					pOrigin = closest.intersectionPoint;
					pDirection = rReflection;
				}
			}
		}
	}

	KDTree photonTree(photons[0]);
	if(!photons.empty()) {
		for(int p = 1; p < photons.size(); p++) {
			photonTree.insert(photonTree.root, photons[p], 0);
			// std::cout << p << " " << n->value[2] << std::endl;
		}
	}
	photonsExist = true;
	std::cout << "photon map built" << std::endl;
	return photonTree;
}

void draw(DrawingWindow &window) {
	for(int x = 0; x < WIDTH; x++) {
		for(int y = 0; y < HEIGHT; y++) {
			ZBuffer[x][y] = 0.0;
		}
	}
	window.clearPixels();
	switch (renderMode)
	{
	case WIREFRAME:
		for(int i=0; i < pairs.size(); i++) {
			ModelTriangle triangle = pairs[i].first;
			Material material = pairs[i].second;

			std::vector<glm::vec3> renderPos;
			for(int i=0; i < triangle.vertices.size(); i++) {
				glm::vec3 vertex = triangle.vertices[i] - camera.pos;
				vertex = camera.rot * vertex;
				float u = glm::floor(-1*camera.f*(vertex.x / vertex.z)*(HEIGHT*1.5)+ WIDTH/2);
				float v = glm::floor(camera.f*(vertex.y / vertex.z)*(HEIGHT*1.5) + HEIGHT/2);
				float Z = INFINITY;
				
				if(vertex.z != 0.0) {
					Z = glm::abs(1 / vertex.z);
				}
				renderPos.push_back(glm::vec3(u, v, Z));
			}
			CanvasTriangle transposedTri = CanvasTriangle(CanvasPoint(renderPos[0].x, renderPos[0].y, renderPos[0].z), CanvasPoint(renderPos[1].x, renderPos[1].y, renderPos[1].z) ,CanvasPoint(renderPos[2].x, renderPos[2].y, renderPos[2].z));
			drawTriangle(window,transposedTri,material.colour);
		}
		break;
	case RASTERIZING:
		for(int i=0; i < pairs.size(); i++) {
			drawModelTriangle(window, pairs[i]);
		}
		break;
	case RAYTRACING:
		if(!photonsExist) PHOTONMAP = photonMap(pairs, 1000000);
		rayTracing(window, pairs, 750.0);
		break;
	default:
		break;
	}
	window.setPixelColour(432,39,0x00FF0000);
	
}
float theta = glm::acos(camera.pos.x / glm::distance(glm::vec3(0.0), camera.pos));

void orbit() {
	float radius = glm::distance(glm::vec3(0.0), camera.pos);
	float interval = glm::radians(5.0);
	theta += interval;
	float newX = radius*glm::cos(theta);
	float newZ = radius*glm::sin(theta);
	camera.pos = glm::vec3(newX, camera.pos.y, newZ);
	camera.rot = lookAt();
}

void update(DrawingWindow &window) {
	if(orbitMode) orbit();

}



int main(int argc, char *argv[]) {
	srand(time(NULL));
	ZBuffer.resize(WIDTH);
	for(int x = 0; x < WIDTH; x++) {
		ZBuffer[x].resize(HEIGHT);
		for(int y = 0; y < HEIGHT; y++) {
			ZBuffer[x][y] = 0.0;
		}
	}

	pairs = loadObj("textured-cornell-box.obj", 0.17);
	std::vector<std::pair<ModelTriangle, Material>> logo = loadObj("logo2.obj", 0.17);
	for(int i = 0; i < logo.size(); i++) {
		pairs.push_back(logo[i]);
	}
	std::vector<std::pair<ModelTriangle, Material>> orb = loadObj("sphere.obj", 0.17);
	for(int i = 0; i < orb.size(); i++) {
		pairs.push_back(orb[i]);
	}
	lightSource = glm::vec3(0, pairs[0].first.vertices[2].y - 0.1, 0.0); 

	DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);
	SDL_Event event;
	int n = 0;
	while (true) {
		if (window.pollForInputEvents(event)) handleEvent(event, window);
		update(window);
		draw(window);

		window.renderFrame();
		window.savePPM("frames/output" + std::to_string(n) + ".ppm");
		n++;
	}
}
