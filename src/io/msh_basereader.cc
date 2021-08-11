/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *
 * @file    msh_basereader.cc
 * @brief
 * @author  dalibor
 */


#include "io/msh_basereader.hh"
#include "io/msh_gmshreader.h"
#include "io/msh_vtkreader.hh"
#include "io/msh_pvdreader.hh"
#include "mesh/mesh.h"
#include "system/sys_profiler.hh"


BaseMeshReader::BaseMeshReader(const FilePath &file_name)
: element_data_values_(std::make_shared<ElementDataFieldMap>()),
  tok_(file_name)
{}

BaseMeshReader::BaseMeshReader(const FilePath &file_name, std::shared_ptr<ElementDataFieldMap> element_data_values)
: element_data_values_(element_data_values),
  tok_(file_name)
{}

std::shared_ptr< BaseMeshReader > BaseMeshReader::reader_factory(const FilePath &file_name) {
	std::shared_ptr<BaseMeshReader> reader_ptr;
	if ( file_name.extension() == ".msh" ) {
		reader_ptr = std::make_shared<GmshMeshReader>(file_name);
	} else if ( file_name.extension() == ".vtu" ) {
		reader_ptr = std::make_shared<VtkMeshReader>(file_name);
	} else if ( file_name.extension() == ".pvd" ) {
		reader_ptr = std::make_shared<PvdMeshReader>(file_name);
	} else {
		THROW(ExcWrongExtension() << EI_FileExtension(file_name.extension()) << EI_MeshFile((string)file_name) );
	}
	return reader_ptr;
}

Mesh * BaseMeshReader::mesh_factory(const Input::Record &input_mesh_rec) {
    START_TIMER("BaseMeshReader - mesh factory");

	Input::Array region_list;
	Mesh * mesh = new Mesh( input_mesh_rec );

	try {
		std::shared_ptr< BaseMeshReader > reader = BaseMeshReader::reader_factory(input_mesh_rec.val<FilePath>("mesh_file"));
		reader->read_physical_names(mesh);
		if (input_mesh_rec.opt_val("regions", region_list)) {
			mesh->read_regions_from_input(region_list);
		}
		reader->read_raw_mesh(mesh);
    } INPUT_CATCH(FilePath::ExcFileOpen, FilePath::EI_Address_String, input_mesh_rec)

    mesh->setup_topology();
    mesh->check_and_finish();
    return mesh;

}

void BaseMeshReader::read_raw_mesh(Mesh * mesh) {
	ASSERT(mesh).error("Argument mesh is NULL.\n");
    tok_.set_position( Tokenizer::Position() );
    read_nodes(mesh);
    read_elements(mesh);
}


std::vector<int> const & BaseMeshReader::get_element_vector(bool boundary_domain) {
	if (boundary_domain) return boundary_elements_id_;
	else return bulk_elements_id_;
}


template<typename T>
typename ElementDataCache<T>::ComponentDataPtr BaseMeshReader::get_element_data(
        MeshDataHeader header, unsigned int expected_n_entities,
        unsigned int expected_n_components, bool boundary_domain) {
	ASSERT(has_compatible_mesh_)
			.error("Vector of mapping VTK to GMSH element is not initialized. Did you call check_compatible_mesh?");

    std::string field_name = header.field_name;

	ElementDataFieldMap::iterator it=element_data_values_->find(field_name);
    if (it == element_data_values_->end()) {
    	(*element_data_values_)[field_name] = std::make_shared< ElementDataCache<T> >();
        it=element_data_values_->find(field_name);
    }

    if ( !it->second->is_actual(header.time, field_name) ) {
    	unsigned int size_of_cache = 1; // count of vectors stored in cache

	    // check that the header is valid - expected_n_entities
	    if (header.n_entities != expected_n_entities) {
	    	WarningOut().fmt("In file '{}', '{}' section for field '{}', time: {}.\nDifferent number of entities: {}, computation needs {}.\n",
	                tok_.f_name(), data_section_name_, field_name, header.time, header.n_entities, expected_n_entities);
	        // header.n_entities = expected_n_entities;
	    }
        // check that the header is valid, try to correct n_components
        if (header.n_components != expected_n_components) {
            WarningOut().fmt("In file '{}', '{}' section for field '{}', time: {}.\nWrong number of components: {}, using {} instead.\n",
                    tok_.f_name(), data_section_name_, field_name, header.time, header.n_components, expected_n_components);
            header.n_components = expected_n_components;
        }

    	(*element_data_values_)[field_name] = std::make_shared< ElementDataCache<T> >(
                field_name, header.time, size_of_cache,
                expected_n_components*expected_n_entities);
    	this->read_element_data(*(it->second), header, boundary_domain );
	}

    ElementDataCache<T> &current_cache = dynamic_cast<ElementDataCache<T> &>(*(it->second));
	return current_cache.get_component_data(0);
}

CheckResult BaseMeshReader::scale_and_check_limits(string field_name, double coef, double default_val, double lower_bound,
        double upper_bound) {
    ElementDataFieldMap::iterator it=element_data_values_->find(field_name);
    ASSERT(it != element_data_values_->end())(field_name);

    std::shared_ptr< ElementDataCache<double> > current_cache = dynamic_pointer_cast<ElementDataCache<double> >(it->second);
    ASSERT(current_cache)(field_name).error("scale_and_check_limits can be call only for scalable fields!\n");

    CheckResult check_val = current_cache->check_values(default_val, lower_bound, upper_bound);
    current_cache->scale_data(coef);
    return check_val;
}



// explicit instantiation of template methods
#define MESH_READER_GET_ELEMENT_DATA(TYPE) \
template typename ElementDataCache<TYPE>::ComponentDataPtr BaseMeshReader::get_element_data<TYPE>( \
        MeshDataHeader header, unsigned int n_entities, \
	    unsigned int n_components, bool boundary_domain);

MESH_READER_GET_ELEMENT_DATA(int)
MESH_READER_GET_ELEMENT_DATA(unsigned int)
MESH_READER_GET_ELEMENT_DATA(double)

