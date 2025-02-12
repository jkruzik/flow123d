/*
 * vtk_output_test.cpp
 *
 *      Author: Jiri Hnidek <jiri.hnidek@tul.cz>
 */

#define TEST_USE_PETSC
#define FEAL_OVERRIDE_ASSERTS
/*
 * NOTE: This unit test uses asserts defined in namespace feal, not asseerts defined
 * in gtest library.
 */
#include <flow_gtest_mpi.hh>
#include <mesh_constructor.hh>

#include "io/output_time.hh"
#include "io/element_data_cache_base.hh"
#include "io/output_mesh.hh"
#include "tools/time_governor.hh"

#include "mesh/mesh.h"
#include "io/msh_gmshreader.h"

#include "input/reader_to_storage.hh"
#include "input/accessors.hh"

#include "system/sys_profiler.hh"
#include "system/file_path.hh"

#include "fields/field.hh"





FLOW123D_FORCE_LINK_IN_PARENT(field_constant)



// Test #1 of input for output stream
const string output_stream1 = R"JSON(
{
  file = "./test1.pvd", 
  format = {
    TYPE = "vtk", 
    variant = "ascii"
  }, 
  name = "flow_output_stream1"
}
)JSON";


// Test #2 of input for output stream
const string output_stream2 = R"JSON(
{
  file = "./test2.msh",
  format = {
    TYPE = "gmsh",
    variant = "ascii"
  }, 
  name = "flow_output_stream2"
}
)JSON";

// Test #1 of input for output stream
const string output_stream3 = R"JSON(
{
  file = "./test3.pvd", 
  format = {
    TYPE = "vtk", 
    variant = "ascii"
  }, 
  name = "flow_output_stream3"
}
)JSON";

// Test input for output data
const string foo_output = R"JSON(
{
  pressure_p0 = "flow_output_stream1",
  material_id = "flow_output_stream1",
  pressure_p1 = "flow_output_stream2",
  strangeness = "flow_output_stream2"
  pressure_p2 = "flow_output_stream3",
  computenode = "flow_output_stream3"
}
)JSON";


namespace IT=Input::Type;

// Comment out following test for two reasons:
// 1) factory function for OutputTime objects may finally not be necessary
//    and possibly will be removed
// 2) Test of protected/private members can not be done simply using fixture class
//    since OutputTime nor its descendants have default constructor.
//    Possible solution may be: Have standard test, declare class that is descendant of e.g. OutputVTK,
//    make instance of that class in the test and have particular test function in the class.

#if 0
/**
 * \brief Child class used for testing OutputTime
 */
class OutputTest : public testing::Test, public OutputTime {
public:
    static IT::Record input_type;

    OutputTest()
    : OutputTime(
    		Input::ReaderToStorage(output_stream1, OutputTime::get_input_type(), Input::FileFormat::format_JSON)
    		.get_root_interface<Input::Record>()
    		)
    {}

    ~OutputTest() {OutputTime::destroy_all();}
};

IT::Record OutputTest::input_type
    = IT::Record("Foo", "Output setting for foo equations.")
    .declare_key("pressure_p0", IT::String(),
            "Name of output stream for P0 approximation of pressure.")
    .declare_key("material_id", IT::String(),
            "Name of output stream for material ID.")
    .declare_key("pressure_p1", IT::String(),
            "Name of output stream for P1 approximation of pressure.")
    .declare_key("strangeness", IT::String(),
            "Name of output stream for strangeness.")
    .declare_key("pressure_p2", IT::String(),
            "Name of output stream for P2 approximation of pressure.")
    .declare_key("computenode", IT::String(),
            "Name of output stream for compute node ID.");


/**
 * \brief Test of creating of OutputTime
 */
TEST_F( OutputTest, test_create_output_stream ) {
	// First stream is created in constructor, here we create two more.
	Input::v reader_2(output_stream2, OutputTime::get_input_type(), Input::FileFormat::format_JSON);
    OutputTime::output_stream(reader_2.get_root_interface<Input::Record>());

    Input::ReaderToStorage reader_3(output_stream3, OutputTime::get_input_type(), Input::FileFormat::format_JSON);
    OutputTime::output_stream(reader_3.get_root_interface<Input::Record>());

    /* Make sure that there are 3 OutputTime instances */
    ASSERT_PERMANENT_EQ(OutputTime::output_streams.size(), 3);


    /* The name of instance has to be equal to configuration file:
     * "variable output_stream" */
    EXPECT_EQ(this->name, "flow_output_stream1");

    /* The type of instance should be OutputVTK */
    EXPECT_EQ(this->file_format, OutputTime::VTK);

    /* The filename should be "./test1.pvd" */
    EXPECT_EQ(*(this->base_filename()), "./test1.pvd");
}
#endif

/**
 *
 */
/*
TEST( OutputTest, find_outputstream_by_name ) {
	Input::ReaderToStorage reader_1(output_stream1, OutputTime::get_input_type(), Input::FileFormat::format_JSON);
    auto os_1 = OutputTime::output_stream(reader_1.get_root_interface<Input::Record>());

	Input::ReaderToStorage reader_2(output_stream2, OutputTime::get_input_type(), Input::FileFormat::format_JSON);
    auto os_2 = OutputTime::output_stream(reader_2.get_root_interface<Input::Record>());

    Input::ReaderToStorage reader_3(output_stream3, OutputTime::get_input_type(), Input::FileFormat::format_JSON);
    auto os_3 = OutputTime::output_stream(reader_3.get_root_interface<Input::Record>());

    //ASSERT_PERMANENT_EQ(OutputTime::output_streams.size(), 3);

    //std::vector<OutputTime*>::iterator output_iter = OutputTime::output_streams.begin();
    //OutputTime *output_time = *output_iter;

    EXPECT_EQ(os_1, OutputTime::output_stream_by_name("flow_output_stream1"));
    EXPECT_EQ(os_3, OutputTime::output_stream_by_name("flow_output_stream3"));
    EXPECT_EQ(nullptr, OutputTime::output_stream_by_name("flow_output_stream4"));
}
*/

////////////////////////////////////////////////////////////////////////////////////
// Test compute_field_data for all possible template parameters.
// this is also test of OutputData<..> internal storage class.
// We test storage of the data and their retrieval by the print method.

const string test_output_time_input = R"JSON(
{
  file = "./test1.pvd", 
  format = {
    TYPE = "vtk", 
    variant = "ascii"
  }, 
  name = "test_output_time_stream"
}
)JSON";

static const Input::Type::Selection & get_test_selection() {
	return Input::Type::Selection("any")
		.add_value(0,"black")
		.add_value(3,"white")
		.close();
}

class TestOutputTime : //public testing::Test,
                       public OutputTime,
					   public std::enable_shared_from_this<TestOutputTime>
{
public:
	TestOutputTime()
	: OutputTime()

	{
	    Profiler::instance();
		// read simple mesh
	    FilePath mesh_file( string(UNIT_TESTS_SRC_DIR) + "/mesh/simplest_cube.msh", FilePath::input_file);
	    my_mesh = mesh_full_constructor("{ mesh_file=\"" + (string)mesh_file + "\", optimize_mesh=false }");

	    auto in_rec =
	            Input::ReaderToStorage(test_output_time_input, const_cast<Input::Type::Record &>(OutputTime::get_input_type()), Input::FileFormat::format_JSON)
                .get_root_interface<Input::Record>();
	    this->init_from_input("dummy_equation", in_rec, std::make_shared<TimeUnitConversion>());

	    component_names = { "comp_0", "comp_1", "comp_2" };

        // create output mesh identical to computational mesh
        output_mesh_ = std::make_shared<OutputMesh>(*my_mesh);
        output_mesh_->create_sub_mesh();
        output_mesh_->make_serial_master_mesh();
        this->set_output_data_caches(output_mesh_);

	}
	virtual ~TestOutputTime() {
	    delete my_mesh;
	}
	int write_data(void) override {return 0;};
	//int write_head(void) override {return 0;};
	//int write_tail(void) override {return 0;};


	// test_compute_field_data
	template <int spacedim, class Value>
	void test_compute_field_data(string init, string result, string rval) {
	    typedef typename Value::element_type ElemType;

	    // make field init it form the init string
	    Field<spacedim, Value> field("test_field"); // bulk field
		field.units(UnitSI::one());
		field.input_default(init);
		field.set_components(component_names);
		field.input_selection( get_test_selection() );

		field.set_mesh(*my_mesh);
		field.set_time(TimeGovernor(0.0, 1.0).step(), LimitSide::left);
        
        //this->output_mesh_discont_ = std::make_shared<OutputMeshDiscontinuous>(*my_mesh);
        //this->output_mesh_discont_->create_sub_mesh();
        //this->output_mesh_discont_->make_serial_master_mesh();
        
		{
            field.set_output_data_cache(OutputTime::ELEM_DATA, shared_from_this());
            auto output_cache_base = this->prepare_compute_data<ElemType>("test_field", OutputTime::ELEM_DATA,
                    (unsigned int)Value::NRows_, (unsigned int)Value::NCols_);
            std::shared_ptr<ElementDataCache<ElemType>> output_data_cache = std::dynamic_pointer_cast<ElementDataCache<ElemType>>(output_cache_base);
            arma::mat ret_value(rval);
            for (uint i=0; i<output_data_cache->n_values(); ++i)
                output_data_cache->store_value(i, ret_value.memptr() );

        	this->gather_output_data();
			EXPECT_EQ(1, output_data_vec_[ELEM_DATA].size());
			OutputDataPtr data =  output_data_vec_[ELEM_DATA][0];
			EXPECT_EQ(my_mesh->n_elements(), data->n_values());
			for(unsigned int i=0;  i < data->n_values(); i++) {
				std::stringstream ss;
				data->print_ascii(ss, i);
				EXPECT_EQ(result, ss.str() );
			}
		}

		{
            field.set_output_data_cache(OutputTime::NODE_DATA, shared_from_this());
            auto output_cache_base = this->prepare_compute_data<ElemType>("test_field", OutputTime::NODE_DATA,
                    (unsigned int)Value::NRows_, (unsigned int)Value::NCols_);
            std::shared_ptr<ElementDataCache<ElemType>> output_data_cache = std::dynamic_pointer_cast<ElementDataCache<ElemType>>(output_cache_base);
            arma::mat ret_value(rval);
            for (uint i=0; i<output_data_cache->n_values(); ++i)
                output_data_cache->store_value(i, ret_value.memptr() );

			this->gather_output_data();
			EXPECT_EQ(1, output_data_vec_[NODE_DATA].size());
			OutputDataPtr data =  output_data_vec_[NODE_DATA][0];
			EXPECT_EQ(my_mesh->n_nodes(), data->n_values());
			for(unsigned int i=0;  i < data->n_values(); i++) {
				std::stringstream ss;
				data->print_ascii(ss, i);
				EXPECT_EQ(result, ss.str() );
			}
		}

		{
			// TODO need fix to discontinuous output data
            /*field.compute_field_data(OutputTime::CORNER_DATA, shared_from_this());
			this->gather_output_data();
			EXPECT_EQ(1, output_data_vec_[CORNER_DATA].size());
			OutputDataPtr data =  output_data_vec_[CORNER_DATA][0];
			//EXPECT_EQ(my_mesh->n_elements(), data->n_values());
			for(unsigned int i=0;  i < data->n_values(); i++) {
				std::stringstream ss;
				data->print_ascii(ss, i);
				EXPECT_EQ(result, ss.str() );
			}*/
		}


		this->clear_data();
		EXPECT_EQ(1, output_data_vec_[NODE_DATA].size());   // filled with DummyElementDataCache
		EXPECT_EQ(1, output_data_vec_[ELEM_DATA].size());   // filled with DummyElementDataCache
		EXPECT_EQ(0, output_data_vec_[CORNER_DATA].size()); // no date at all

		/*

		compute_field_data(OutputTime::NODE_DATA, field);
		EXPECT_EQ(1, node_data.size());
		check_node_data( node_data[0], result);

		compute_field_data(OutputTime::CORNER_DATA, field);
		EXPECT_EQ(1, elem_data.size());
		check_elem_data( elem_data[0], result);
*/
	}

	std::string base_filename() {
		return string(this->_base_filename);
	}

	void base_filename(std::string file_name) {
		this->_base_filename = FilePath(file_name, FilePath::output_file);
	}

	void test_fix_main_file_extension(std::string extension) {
		this->fix_main_file_extension(extension);
	}

	Mesh * my_mesh;
	std::vector<string> component_names;
	std::shared_ptr<OutputMeshBase> output_mesh_;
};



TEST(TestOutputTime, fix_main_file_extension)
{
    std::shared_ptr<TestOutputTime> output_time = std::make_shared<TestOutputTime>();

    output_time->base_filename("test.pvd");
    output_time->test_fix_main_file_extension(".pvd"); // call protected fix_main_file_extension
    EXPECT_EQ("test.pvd", output_time->base_filename());

    output_time->base_filename("test");
    output_time->test_fix_main_file_extension(".pvd");
    EXPECT_EQ("test.pvd", output_time->base_filename());

    output_time->base_filename("test.msh");
    output_time->test_fix_main_file_extension(".pvd");
    EXPECT_EQ("test.pvd", output_time->base_filename());

    output_time->base_filename("test.msh");
    output_time->test_fix_main_file_extension(".msh");
    EXPECT_EQ("test.msh", output_time->base_filename());

    output_time->base_filename("test");
    output_time->test_fix_main_file_extension(".msh");
    EXPECT_EQ("test.msh", output_time->base_filename());

    output_time->base_filename("test.pvd");
    output_time->test_fix_main_file_extension(".msh");
    EXPECT_EQ("test.msh", output_time->base_filename());

}


#define FV FieldValue
TEST(TestOutputTime, compute_field_data) {
	std::shared_ptr<TestOutputTime> output_time = std::make_shared<TestOutputTime>();
	output_time->test_compute_field_data<3, FV<0>::Scalar>("1.3", "1.3 ", "1.3 ");
	//output_time->test_compute_field_data<3, FV<0>::Enum>("\"white\"", "3 ", "3 ");
	//output_time->test_compute_field_data<3, FV<0>::Integer>("3", "3 ", "3 ");
	output_time->test_compute_field_data<3, FV<3>::VectorFixed>("[1.2, 3.4, 5.6]", "1.2 3.4 5.6 ", "1.2 3.4 5.6 ");
	//output_time->test_compute_field_data<3, FV<2>::VectorFixed>("[1.2, 3.4]", "1.2 3.4 0 ", "1.2 3.4 0 ");
	output_time->test_compute_field_data<3, FV<3>::TensorFixed>("[[1, 2, 0], [2, 4, 3], [0, 3, 5]]", "1 2 0 2 4 3 0 3 5 ", "1 2 0; 2 4 3; 0 3 5 ");
	//output_time->test_compute_field_data<3, FV<2>::TensorFixed>("[[1, 2], [4,5]]", "1 2 0 4 5 0 0 0 0 ", "1 2 0; 4 5 0; 0 0 0 ");
}






#if 0

TEST_F( OutputTest, test_register_elem_fields_data ) {
    /* Read input for output */
    Input::ReaderToStorage reader_output(foo_output, Foo::input_type, Input::FileFormat::format_JSON);

    TimeGovernor tg(0.0, 1.0);

    Profiler::instance();

    FilePath::set_io_dirs(".", UNIT_TESTS_SRC_DIR, "", ".");

    Mesh mesh;
    ifstream in(string( FilePath("mesh/simplest_cube.msh", FilePath::input_file) ).c_str());
    mesh.read_gmsh_from_stream(in);

    Field<3, FieldValue<1>::Scalar> scalar_field;

    /* Initialization of scalar field  with constant double values (1.0) */
    scalar_field.input_default( "2" );
    scalar_field.name("pressure_p0");
    scalar_field.units("L");
    scalar_field.set_mesh(mesh);
    scalar_field.set_time(tg.step(), LimitSide::right);

    /* Register scalar (double) data */
    OutputTime::register_data<3, FieldValue<1>::Scalar>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::ELEM_DATA, &scalar_field);

    Field<3, FieldValue<1>::Integer> integer_field;

    /* Initialization of scalar field  with constant double values (1.0) */
    integer_field.input_default( "10" );
    integer_field.name("material_id");
    integer_field.units("");
    integer_field.set_mesh(mesh);
    integer_field.set_time(tg.step(), LimitSide::right);

    /* Register integer data */
    OutputTime::register_data<3, FieldValue<1>::Integer>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::ELEM_DATA, &integer_field);

    /* There should be three output streams */
    ASSERT_PERMANENT_EQ(OutputTime::output_streams.size(), 3);

    /* Get first OutputTime instance */
    std::vector<OutputTime*>::iterator output_iter = OutputTime::output_streams.begin();
    OutputTime *output_time = *output_iter;

    EXPECT_EQ(output_time, OutputTime::output_stream_by_name("flow_output_stream1"));

    /* There should be two items in vector of registered element data */
    ASSERT_PERMANENT_EQ(output_time->elem_data.size(), 2);

    /* Get first registered data */
    std::vector<ElementDataCacheBase*>::iterator output_data_iter = output_time->elem_data.begin();
    ElementDataCacheBase *output_data = *output_data_iter;

    /* Number of items should be equal to number of mesh elements */
    EXPECT_EQ(output_data->items_count, mesh.n_elements());

    /* All values has to be equal 2.0 */
    for(int i = 0; i < mesh.n_elements(); i++) {
        EXPECT_EQ((*(OutputData<double>*)output_data)[i], 2.0);
    }

    /* Get next registered data */
    output_data = *(++output_data_iter);

    /* Number of items should be equal to number of mesh elements */
    EXPECT_EQ(output_data->items_count, mesh.n_elements());

    /* All values has to be equal 10 */
    for(int i = 0; i < mesh.n_elements(); i++) {
        EXPECT_EQ((*(OutputData<int>*)output_data)[i], 10);
    }

    /* Try to write data to output file */
    OutputTime::write_all_data();
}

TEST_F( OutputTest, test_register_corner_fields_data ) {
    /* Read input for output */
    Input::ReaderToStorage reader_output(foo_output, Foo::input_type, Input::FileFormat::format_JSON);
    TimeGovernor tg(0.0, 1.0);

    Profiler::instance();

    FilePath::set_io_dirs(".", UNIT_TESTS_SRC_DIR, "", ".");

    Mesh mesh;
    ifstream in(string( FilePath("mesh/simplest_cube.msh", FilePath::input_file) ).c_str());
    mesh.read_gmsh_from_stream(in);

    Field<3, FieldValue<1>::Scalar> scalar_field;

    /* Initialization of scalar field  with constant double values (1.0) */
    scalar_field.input_default( "20" );
    scalar_field.name("pressure_p1");
    scalar_field.units("L");
    scalar_field.set_mesh(mesh);
    scalar_field.set_time(tg.step(), LimitSide::right);

    /* Register scalar (double) data */
    OutputTime::register_data<3, FieldValue<1>::Scalar>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::CORNER_DATA, &scalar_field);

    Field<3, FieldValue<1>::Integer> integer_field;

    /* Initialization of scalar field  with constant double values (1.0) */
    integer_field.input_default( "-1" );
    integer_field.name("strangeness");
    integer_field.units("");
    integer_field.set_mesh(mesh);
    integer_field.set_time(tg.step(), LimitSide::right);

    /* Register integer data */
    OutputTime::register_data<3, FieldValue<1>::Integer>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::CORNER_DATA, &integer_field);

    /* There should three output streams */
    ASSERT_PERMANENT_EQ(OutputTime::output_streams.size(), 3);

    /* Get this OutputTime instance */
    std::vector<OutputTime*>::iterator output_iter = OutputTime::output_streams.begin();
    OutputTime *output_time = *output_iter;

    /* Get second output stream */
    output_time = *(++output_iter);

    EXPECT_EQ(output_time, OutputTime::output_stream_by_name("flow_output_stream2"));

    /* There should be two items in vector of registered element data */
    ASSERT_PERMANENT_EQ(output_time->corner_data.size(), 2);

    /* Get first registered corner data */
    std::vector<ElementDataCacheBase*>::iterator output_data_iter = output_time->corner_data.begin();
    ElementDataCacheBase *output_data = *output_data_iter;

    /* All values has to be equal 20.0 */
    Node *node;
    int node_id;
    int corner_data_count, corner_id = 0;
    for (auto ele : mesh->elements_range()) {
    	for (node_id=0; node_id<ele->n_nodes(); node_id++) {
            EXPECT_EQ((*(OutputData<double>*)output_data)[corner_id], 20.0);
            corner_id++;
        }
    }

    corner_data_count = corner_id;

    /* Number of items should be equal to number of mesh elements */
    EXPECT_EQ(output_data->items_count, corner_data_count);

    /* Get next registered corner data */
    output_data = *(++output_data_iter);

    /* Number of items should be equal to number of mesh elements */
    EXPECT_EQ(output_data->items_count, corner_data_count);

    /* All values has to be equal 100 */
    corner_id = 0;
    for (auto ele : mesh->elements_range()) {
    	for (node_id=0; node_id<ele->n_nodes(); node_id++) {
            EXPECT_EQ((*(OutputData<int>*)output_data)[corner_id], -1);
            corner_id++;
        }
    }

    /* Try to write data to output file */
    OutputTime::write_all_data();

}

TEST_F( OutputTest, test_register_node_fields_data ) {
    /* Read input for output */
    Input::ReaderToStorage reader_output(foo_output, Foo::input_type, Input::FileFormat::format_JSON);
    TimeGovernor tg(0.0, 1.0);

    Profiler::instance();

    FilePath::set_io_dirs(".", UNIT_TESTS_SRC_DIR, "", ".");

    Mesh mesh;
    ifstream in(string( FilePath("mesh/simplest_cube.msh", FilePath::input_file) ).c_str());
    mesh.read_gmsh_from_stream(in);

    Field<3, FieldValue<1>::Scalar> scalar_field;

    /* Initialization of scalar field  with constant double values (1.0) */
    scalar_field.input_default( "20" );
    scalar_field.name("pressure_p2");
    scalar_field.units("L");
    scalar_field.set_mesh(mesh);
    scalar_field.set_limit_side(LimitSide::right);
    scalar_field.set_time(tg.step());

    /* Register scalar (double) data */
    OutputTime::register_data<3, FieldValue<1>::Scalar>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::NODE_DATA, &scalar_field);

    Field<3, FieldValue<1>::Integer> integer_field;

    /* Initialization of scalar field  with constant int values (0) */
    integer_field.input_default( "2" );
    integer_field.name("computenode");
    integer_field.units("");
    integer_field.set_mesh(mesh);
    integer_field.set_time(tg.step(),LimitSide::right);

    /* Register integer data */
    OutputTime::register_data<3, FieldValue<1>::Integer>(reader_output.get_root_interface<Input::Record>(),
            OutputTime::NODE_DATA, &integer_field);

    /* There should three output streams */
    ASSERT_PERMANENT_EQ(OutputTime::output_streams.size(), 3);

    /* Get this OutputTime instance */
    std::vector<OutputTime*>::iterator output_iter = OutputTime::output_streams.begin();
    OutputTime *output_time = *output_iter;

    /* Get third output stream */
    ++output_iter;
    output_time = *(++output_iter);

    EXPECT_EQ(output_time, OutputTime::output_stream_by_name("flow_output_stream3"));

    /* There should be two items in vector of registered element data */
    ASSERT_PERMANENT_EQ(output_time->node_data.size(), 2);

    /* Get first registered corner data */
    std::vector<ElementDataCacheBase*>::iterator output_data_iter = output_time->node_data.begin();
    ElementDataCacheBase *output_data = *output_data_iter;

    /* Number of items should be equal to number of mesh nodes */
    EXPECT_EQ(output_data->items_count, mesh.n_nodes());

    /* All values has to be equal 20.0 */
    for(int node_id=0; node_id < mesh.n_nodes(); node_id++) {
        EXPECT_EQ((*(OutputData<double>*)output_data)[node_id], 20.0);
    }

    /* Get next registered corner data */
    output_data = *(++output_data_iter);

    /* Number of items should be equal to number of mesh nodes */
    EXPECT_EQ(output_data->items_count, mesh.n_nodes());

    /* All values has to be equal 2 */
    for(int node_id=0; node_id < mesh.n_nodes(); node_id++) {
        EXPECT_EQ((*(OutputData<int>*)output_data)[node_id], 2);
    }

    /* Try to write data to output file */
    OutputTime::write_all_data();

}

#endif




