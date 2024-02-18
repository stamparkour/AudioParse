#include <fstream>
#include <vector>
#include <stdio.h>
#include <math.h>
#include <iostream>


float toHz(char name[3]) {
	if (name[0] == '0') return 0;

	int off = 0;
	if (name[1] == 's') off++;
	if (name[1] == 'f') off--;
	int o = name[off ? 2 : 1] - '0';

	int note = 0;
	switch (name[0]) {
	case 'A': note = 0; break;
	case 'B': note = 2; break;
	case 'C': note = 3; break;
	case 'D': note = 5; break;
	case 'E': note = 7; break;
	case 'F': note = 8; break;
	case 'G': note = 10; break;
	default: throw 3;
	}

	note += off;

	if (name[0] >= 'C') note -= 12;
	note += (o - 4) * 12;
	return 440.0f * powf(2, note / 12.0f);
}

struct Note {
	char name[3];
	float length;

	int Append(char* str, size_t s, float& time, float& hz) {
		float t = time;
		time += length;
		float h = toHz(name);
		float j = h - hz;
		hz = h;
		return sprintf_s(str, s, "(%f,%f)", t, j);
	}
};

std::vector<Note> notes{};
char out[16384];
int main(int argc, const char* argv[]) {
	std::cout << "enter file path:" << std::endl;
	char file[256];
	std::cin >> file;
	std::fstream s {file,std::ios::in};
	if (!s)
		throw 2;
	while (s.good()) {
		char str[256];
		s.getline(str, 256);

		char name[3];
		float length;

		if (str[0] == 0) continue;
		if(sscanf_s(str, "%3c %f", name, 3, &length) != 2) 
			throw 1;
		
		Note n{};
		n.name[0] = name[0];
		n.name[1] = name[1];
		n.name[2] = name[2];
		n.length = length;
		notes.push_back(n);
	}

	int index = 0;
	out[index++] = '[';
	float hz = 0;
	float time = 0;
	for (size_t i = 0; i < notes.size(); i++) {
		index += notes[i].Append(out + index, 16384 - index, time, hz);
		if (i != notes.size() - 1) out[index++] = ',';
	}
	out[index++] = ']';
	out[index] = 0;
	printf(out);
	char arr[256];
	std::cin >> arr;
}