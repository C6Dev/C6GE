#pragma once

namespace C6GE {

	struct VertexShaderComponent {
		const char* shaderCode;
		VertexShaderComponent(const char* code) : shaderCode(code) {}
	};

	struct FragmentShaderComponent {
		const char* shaderCode;
		FragmentShaderComponent(const char* code) : shaderCode(code) {}
	};

	struct CompiledVertexShaderComponent {
		unsigned int shaderID;
		CompiledVertexShaderComponent(unsigned int id) : shaderID(id) {}
	};

	struct CompiledFragmentShaderComponent {
		unsigned int shaderID;
		CompiledFragmentShaderComponent(unsigned int id) : shaderID(id) {}
	};

}