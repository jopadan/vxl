#include <vcl_compiler.h>
#include <iostream>
#include <string>
#include <vbl/vbl_array_3d.hxx>
#include <boxm/boxm_block.hxx>
#include <boct/boct_tree.hxx>

typedef boct_tree<short, char > tree_type;
VBL_ARRAY_3D_INSTANTIATE(boxm_block<tree_type>*);

