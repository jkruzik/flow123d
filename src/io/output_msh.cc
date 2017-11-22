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
 * @file    output_msh.cc
 * @brief   The functions for outputs to GMSH files.
 */

#include "output_msh.hh"
#include "output_mesh.hh"
#include "output_element.hh"
#include "mesh/mesh.h"
#include "element_data_cache_base.hh"
#include "input/factory.hh"


FLOW123D_FORCE_LINK_IN_CHILD(gmsh)


using namespace Input::Type;


/**
 * Auxiliary implementation of ElementDataCacheBase that performs output of single zero data for the fields that are
 * off for current time frame.
 */
class DummyOutputData : public ElementDataCacheBase {
public:

    DummyOutputData(std::string field_name_in, ElementDataCacheBase::NumCompValueType n_elem_in)
   {
        this->field_input_name_ = field_name_in;
        this->n_elem_ = n_elem_in;
        this->n_values_ = 1;
    }

    virtual ~DummyOutputData() override
    {}

    void print_ascii(ostream &out_stream, unsigned int idx) override
    {
        for(unsigned int i=0; i< n_elem_;i++) out_stream << 0 << " ";
    }

    void print_ascii_all(ostream &out_stream) override
    {
        for(unsigned int i=0; i< n_elem_;i++) out_stream << 0 << " ";
    }

    void print_binary_all(ostream &out_stream, bool print_data_size = true) override
    {
        ASSERT(false).error("Not implemented.");
    }

    void print_all_yaml(ostream &out_stream, unsigned int precision) override
    {}

    void get_min_max_range(double &min, double &max) override
    {}

    void read_ascii_data(Tokenizer &tok, unsigned int n_components, unsigned int i_row) override
    {}

    void read_binary_data(std::istream &data_stream, unsigned int n_components, unsigned int i_row) override
    {}
};







const Record & OutputMSH::get_input_type() {
	return Record("gmsh", "Parameters of gmsh output format.")
		// It is derived from abstract class
		.derive_from(OutputTime::get_input_format_type())
		.close();
}

const int OutputMSH::registrar = Input::register_class< OutputMSH >("gmsh") +
		OutputMSH::get_input_type().size();


OutputMSH::OutputMSH()
{
    this->enable_refinement_ = false;
    this->header_written = false;

    dummy_data_list_.resize(OutputTime::N_DISCRETE_SPACES);


}

OutputMSH::~OutputMSH()
{
    this->write_tail();
}




void OutputMSH::write_msh_header(void)
{
    ofstream &file = this->_base_file;

    // Write simple header
    file << "$MeshFormat" << endl;
    file << "2" << " 0 " << sizeof(double) << endl;
    file << "$EndMeshFormat" << endl;
}

void OutputMSH::write_msh_geometry(void)
{
    ofstream &file = this->_base_file;
    Mesh* mesh = this->_mesh;

    // Write information about nodes
    file << "$Nodes" << endl;
    file <<  mesh->node_vector.size() << endl;
    FOR_NODES(mesh, nod) {
        file << NODE_FULL_ITER(mesh, nod).id() << " " << nod->getX() << " " << nod->getY() << " " << nod->getZ() << endl;
    }
    file << "$EndNodes" << endl;
}

void OutputMSH::write_msh_topology(void)
{
    ofstream &file = this->_base_file;
    Mesh* mesh = this->_mesh;
    const static unsigned int gmsh_simplex_types_[4] = {0, 1, 2, 4};
    auto &id_elem_vec = *( this->output_mesh_->get_element_ids_cache()->get_component_data(0).get() );

    // Write information about elements
    file << "$Elements" << endl;
    file << this->output_mesh_->n_elements() << endl;
    auto it = this->output_mesh_->begin();
    ElementAccessor<OutputElement::spacedim> elm;
    for(unsigned int i_elm=0; i_elm < id_elem_vec.size(); ++i_elm, ++it) {
    	elm = it->element_accessor();
        // element_id element_type 3_other_tags material region partition
        file << id_elem_vec[i_elm]
             << " " << gmsh_simplex_types_[ elm.dim() ]
             << " 3 " << elm.region().id() << " " << elm.region().id() << " " << elm.element()->pid;

        for(unsigned int i=0; i<elm.element()->n_nodes(); i++)
            file << " " << NODE_FULL_ITER(mesh, elm.element()->node[i]).id();
        file << endl;
    }
    file << "$EndElements" << endl;
}


void OutputMSH::write_msh_ascii_data(std::shared_ptr<ElementDataCache<unsigned int>> id_cache, OutputDataPtr output_data, bool discont)
{
    ofstream &file = this->_base_file;
    auto &id_vec = *( id_cache->get_component_data(0).get() );

    /* Set precision to max */
    file.precision(std::numeric_limits<double>::digits10);

    if (discont) { // corner data
        auto it = this->output_mesh_->begin();
        unsigned int i_corner = 0;
        for(unsigned int i=0; i < id_vec.size(); ++i, ++it) {
            file << id_vec[i] << " " << it->n_nodes() << " ";
            for (unsigned int j=0; j<it->n_nodes(); j++)
            	output_data->print_ascii(file, i_corner++);
            file << std::endl;
        }
    } else { // element / node data
        for(unsigned int i=0; i < output_data->n_values(); ++i) {
            file << id_vec[i] << " ";
            output_data->print_ascii(file, i);
            file << std::endl;
        }

    }
}


void OutputMSH::write_node_data(OutputDataPtr output_data)
{
    ofstream &file = this->_base_file;
    double time_fixed = isfinite(this->time)?this->time:0;


    file << "$NodeData" << endl;

    file << "1" << endl;     // one string tag
    file << "\"" << output_data->field_input_name() <<"\"" << endl;

    file << "1" << endl;     // one real tag
    file << time_fixed << endl;    // first real tag = time

    file << "3" << endl;     // 3 integer tags
    file << this->current_step << endl;    // step number (start = 0)
    file << output_data->n_elem() << endl;   // number of components
    file << output_data->n_values() << endl;  // number of values

    this->write_msh_ascii_data(this->output_mesh_->get_node_ids_cache(), output_data);

    file << "$EndNodeData" << endl;
}


void OutputMSH::write_corner_data(OutputDataPtr output_data)
{
    ofstream &file = this->_base_file;
    double time_fixed = isfinite(this->time)?this->time:0;

    file << "$ElementNodeData" << endl;

    file << "1" << endl;     // one string tag
    file << "\"" << output_data->field_input_name() <<"\"" << endl;

    file << "1" << endl;     // one real tag
    file << time_fixed << endl;    // first real tag = time

    file << "3" << endl;     // 3 integer tags
    file << this->current_step << endl;    // step number (start = 0)
    file << output_data->n_elem() << endl;   // number of components
    file << this->_mesh->n_elements() << endl; // number of values

    this->write_msh_ascii_data(this->output_mesh_->get_element_ids_cache(), output_data, true);

    file << "$EndElementNodeData" << endl;
}

void OutputMSH::write_elem_data(OutputDataPtr output_data)
{
    ofstream &file = this->_base_file;
    double time_fixed = isfinite(this->time)?this->time:0;

    file << "$ElementData" << endl;

    file << "1" << endl;     // one string tag
    file << "\"" << output_data->field_input_name() <<"\"" << endl;

    file << "1" << endl;     // one real tag
    file << time_fixed << endl;    // first real tag = time

    file << "3" << endl;     // 3 integer tags
    file << this->current_step << endl;    // step number (start = 0)
    file << output_data->n_elem() << endl;   // number of components
    file << output_data->n_values() << endl;  // number of values

    this->write_msh_ascii_data(this->output_mesh_->get_element_ids_cache(), output_data);

    file << "$EndElementData" << endl;
}

int OutputMSH::write_head(void)
{
	LogOut() << __func__ << ": Writing output file " << this->_base_filename << " ... ";

    this->write_msh_header();

    this->write_msh_geometry();

    this->write_msh_topology();

    LogOut() << "O.K.";

    return 1;
}

int OutputMSH::write_data(void)
{
    // Write header with mesh, when it hasn't been written to output file yet
    if(this->header_written == false) {
        if(this->rank == 0) {
            this->fix_main_file_extension(".msh");
            try {
                this->_base_filename.open_stream( this->_base_file );
            } INPUT_CATCH(FilePath::ExcFileOpen, FilePath::EI_Address_String, input_record_)
        }

        this->write_head();
        this->header_written = true;
    }

    LogOut() << __func__ << ": Writing output file " << this->_base_filename << " ... ";


    auto &node_data_list = this->output_data_vec_[NODE_DATA];
    for(auto data_it = node_data_list.begin(); data_it != node_data_list.end(); ++data_it) {
    	write_node_data(*data_it);
    }
    auto &corner_data_list = this->output_data_vec_[CORNER_DATA];
    for(auto data_it = corner_data_list.begin(); data_it != corner_data_list.end(); ++data_it) {
    	write_corner_data(*data_it);
    }
    auto &elem_data_list = this->output_data_vec_[ELEM_DATA];
    for(auto data_it = elem_data_list.begin(); data_it != elem_data_list.end(); ++data_it) {
    	write_elem_data(*data_it);
    }

    // Flush stream to be sure everything is in the file now
    this->_base_file.flush();

    LogOut() << "O.K.";

    return 1;
}



int OutputMSH::write_tail(void)
{
    return 1;
}



void OutputMSH::add_dummy_fields()
{
	const std::vector<OutputTime::DiscreteSpace> space_types = {OutputTime::NODE_DATA, OutputTime::CORNER_DATA, OutputTime::ELEM_DATA};
	for (auto type_idx : space_types) {
	    auto &dummy_data_list = dummy_data_list_[type_idx];
	    auto &data_list = this->output_data_vec_[type_idx];

        // Collect all output fields
		if (dummy_data_list.size() == 0)
			for(auto out_ptr : data_list)
				dummy_data_list.push_back( std::make_shared<DummyOutputData>(out_ptr->field_input_name(), out_ptr->n_elem()));

	    auto data_it = data_list.begin();
	    for(auto dummy_it = dummy_data_list.begin(); dummy_it != dummy_data_list.end(); ++dummy_it) {
	        if ( data_it == data_list.end() ) {
	        	data_list.push_back( *dummy_it );
	        } else if ((*dummy_it)->field_input_name() == (*data_it)->field_input_name()) {
	        	++data_it;
	        } else {
	        	data_list.push_back( *dummy_it );
	        }
	    }
	}
}


