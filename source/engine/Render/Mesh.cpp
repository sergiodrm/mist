
#include "Mesh.h"
#include "Core/Debug.h"

namespace Mist
{
	void cMesh::Destroy()
	{
		vb = nullptr;
		ib = nullptr;
		primitiveArray.Delete();
	}
}