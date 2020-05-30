#include <gl/GL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Texture
{
	GLuint id;
};

struct Color
{
	float r, g, b;
};

void setupOpenGL()
{
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void createTextureFromImage(Texture* out, const char* filePath)
{
	int width, height, channels;
	unsigned char* imageData = stbi_load(filePath, &width, &height, &channels, 0);
	if (imageData) {
		GLuint textureID;
		glGenTextures(1, &textureID);
		GLenum pixelFormat = (channels == 4)? GL_RGBA : GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, pixelFormat, width, height, 0, pixelFormat, GL_UNSIGNED_BYTE, imageData);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(imageData);

		out->id = textureID;
	}
}

void renderImage(Texture texture, float x, float y, float width, float height)
{
	glBindTexture(GL_TEXTURE_2D, texture.id);
	// Draw a quad with two triangles
	glBegin(GL_TRIANGLE_STRIP);
	// Bottom left
	glTexCoord2f(0, 1);
	glVertex3f(-1+x, -1+y, 0);
	// Bottom right
	glTexCoord2f(1, 1);
	glVertex3f(-1+x+width, -1+y, 0);
	// Top left
	glTexCoord2f(0, 0);
	glVertex3f(-1+x, -1+y+height, 0);
	// Top right
	glTexCoord2f(1, 0);
	glVertex3f(-1+x+width, -1+y+height, 0);
	glEnd();
}