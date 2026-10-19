/*
	Dimension Engine V2
	(c) LifesMinder

	Module name:
		project.cpp
	Purpose:
		This file is the main functionality of the Dimension engine
*/

#include "pch.h"

Project::Project() {
	this->name = L"Untitled";
}

Project::Project(wstring name, map<wstring, any> options) {
	this->name = name;
	this->options = options;
}

Project::~Project() {
	this->name = L"Untitled";
	this->options.clear();
}

bool Project::IsInitialized() {
	auto positive = [this](const wchar_t* key) {
		auto it = options.find(key);
		if (it == options.end()) return false;
		try { return any_cast<int>(it->second) > 0; }
		catch (...) { return false; }
	};
	return positive(L"width") && positive(L"height") && positive(L"fps");
}