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
 * @file    field_common.hh
 * @brief   
 */

#ifndef FIELD_COMMON_HH_
#define FIELD_COMMON_HH_

#include <algorithm>                                   // for sort, unique
#include <limits>                                      // for numeric_limits
#include <memory>                                      // for shared_ptr
#include <ostream>                                     // for operator<<
#include <string>                                      // for string, basic_...
#include <utility>                                     // for make_pair, pair
#include <vector>                                      // for vector, allocator
#include "fields/field_algo_base.hh"                   // for FieldResult
#include "fields/field_flag.hh"                        // for FieldFlag, Fie...
#include "fields/field_values.hh"                      // for FieldEnum
#include "tools/unit_si.hh"                           // for UnitSI
#include "input/accessors.hh"                          // for Record, Array ...
#include "input/input_exception.hh"                    // for ExcInputMessage
#include "input/type_base.hh"                          // for Array, Type
#include "input/type_generic.hh"                       // for Instance
#include "input/type_record.hh"                        // for Record
#include "input/type_selection.hh"                     // for Selection
#include "io/output_time.hh"                           // for OutputTime
#include "io/output_time_set.hh"                       // for OutputTimeSet
#include "mesh/region.hh"                              // for Region (ptr only)
#include "system/asserts.hh"                           // for Assert, ASSERT_PERMANENT
#include "system/exc_common.hh"                        // for EI_Message
#include "system/exceptions.hh"                        // for operator<<
#include "system/flag_array.hh"                        // for FlagArray<>::Mask
#include "tools/time_governor.hh"                      // for TimeGovernor (...

class Mesh;
class Observe;
class EvalPoints;
class ElementCacheMap;
class FieldSet;
class BulkPoint;
class ElementDataCacheBase;


using namespace std;

namespace IT=Input::Type;

/**
 *  Left and right time limit, used in the @p set_time() method.
 *  Assigned values allow to index an array.
 */
enum class LimitSide {
    left=0,
    right=1
};



/**
 * @brief Common abstract parent of all Field<...> classes.
 *
 * We need common ancestor in order to keep a list of all fields in one EqData object and allow
 * collective operations like @p set_time or @p init_from_input.
 */
class FieldCommon {

public:
    TYPEDEF_ERR_INFO(EI_Time, double);
    TYPEDEF_ERR_INFO(EI_Field, std::string);
    TYPEDEF_ERR_INFO( EI_FieldInputName, std::string);
    TYPEDEF_ERR_INFO( EI_FieldName, std::string);
    TYPEDEF_ERR_INFO( EI_RegId, unsigned int);
    TYPEDEF_ERR_INFO( EI_RegLabel, std::string);
    DECLARE_INPUT_EXCEPTION(ExcNonascendingTime,
            << "Non-ascending time: " << EI_Time::val << " for field " << EI_Field::qval << ".\n");
    DECLARE_INPUT_EXCEPTION(ExcMissingDomain,
            << "Missing domain specification (region or r_id) in the field descriptor:");
    DECLARE_EXCEPTION(ExcFieldMeshDifference,
            << "Two copies of the field " << EI_Field::qval << "call set_mesh with different arguments.\n");
    DECLARE_INPUT_EXCEPTION(ExcMissingFieldValue,
            << "Missing value of the input field " << EI_FieldInputName::qval << " (" << EI_FieldName::qval
            << ") on region ID: " << EI_RegId::val << " label: " << EI_RegLabel::qval << ".\n");



	/// Store data of one initialization message.
	struct MessageData {
		/// Constructor
		MessageData(std::string default_value, std::string field_name, std::string region_list)
		: default_value_(default_value), field_name_(field_name), region_list_(region_list) {};

		std::string default_value_;        ///< Default value of the field.
		std::string field_name_;           ///< Parameter name_ of the field.
		std::string region_list_;          ///< List of regions separated by comma.
	};

	/**
     *  Set name of the field. In fact there are two attributes set by this method.
     *
     *  The first is name used to identify the field as part of a FieldSet or MultiField objects.
     *  This name is permanent and can be set only by this method. Can be accessed by @p name() method.
     *  This name is also used at output.
     *
     *  The second is @p input_name_ that determines appropriate key name in the input field descriptor.
     *  This name is also set by this method, but is stored in the internal shared space which
     *  is overwritten during call of copy_from method or  assignment operator. Can be accessed by @p input_name() mathod.
     *
     */
    FieldCommon &name(const string & name)
    { name_=shared_->input_name_ = name;
      return *this;
    }
    /**
     * Set description of the field, used for description of corresponding key in documentation.
     */
    FieldCommon & description(const string & description)
    { shared_->input_description_ = description; return *this;}
    /**
     * Set default value for the field's key from which the default constant valued field will be constructed.
     *
     * During the first call of the @p set_time method, we check that the field is defined on all regions.
     * On regions where it is not set yet, we use given @p dflt string to get particular instance of
     * FieldBase<> (see @p check_initialized_region_fields_).
     * The default string is interpreted in the same way as if it appears in the input file
     * as the value of the field. In particular it can be whole record with @p TYPE of the field etc.
     * Most common choice is however mere constant.
     */
    FieldCommon & input_default(const string &input_default)
    { shared_->input_default_ = input_default; return *this;}
    /**
     * @brief Set basic units of the field.
     *
     * Currently, we use it only during output and we represents units just by a string.
     *
     * TODO:
     * Particular class for representing and conversion of various units would be more appropriate.
     * This can allow specification of the units on the inptu, automatic conversion and the same on the output.
     * Possibly this allow using Boost::Units library, however, it seems to introduce lot of boilerplate code.
     * But can increase correctness of the calculations.
     */
    FieldCommon & units(const UnitSI & units)
    { shared_->units_ = units; return *this;}

    /**
     * Set limits of value of the field.
     */
    FieldCommon & set_limits(double min, double max = std::numeric_limits<double>::max())
    {
    	ASSERT_PERMANENT_LT(min, max).error("Invalid field limits!");
    	shared_->limits_ = std::make_pair(min, max);
    	return *this;
    }

    /**
     * For the fields returning "Enum", we have to pass the Input::Type::Selection object to
     * the field implementations.
     *
     * We must save raw pointer since selection may not be yet initialized (during static initialization phase).
     */
    FieldCommon & input_selection(Input::Type::Selection element_selection)
    {
      shared_->input_element_selection_=element_selection;
      return *this;
    }

    /**
     * Output discrete space used in the output() method. Can be different for different field copies.
     * one can choose between:
     * data constant on elements, linear data given in nodes, and discontinuous linear data.
     *
     * If not set explicitly by this method, the default value is OutputTime::ELEM_DATA
     */
    FieldCommon & output_type(OutputTime::DiscreteSpace rt)
    { if (rt!=OutputTime::UNDEFINED) default_output_data_ = rt; return *this; }

    /**
     * Set given mask to the field flags, ignoring default setting.
     * Default setting is declare_input & equation_input & allow_output.
     */
    FieldCommon & flags(FieldFlag::Flags::Mask mask)
    { flags_ = FieldFlag::Flags(mask); return *this; }

    /**
     * Add given mask to the field flags.
     */
    FieldCommon & flags_add(FieldFlag::Flags::Mask mask)
    { flags().add(mask); return *this; }

    /**
     * Set vector of component names.
     * Set number of components for run-time sized vectors. This is used latter when we construct
     * objects derived from FieldBase<...>.
     *
     * n_comp_ is constant zero for fixed values, this zero is set by Field<...> constructors
     */
    void set_components(const std::vector<string> &names) {
    	// Test of unique values in names vector for MultiField
    	if (multifield_) {
			std::vector<string> cpy = names;
			std::sort( cpy.begin(), cpy.end() );
			cpy.erase( std::unique( cpy.begin(), cpy.end() ), cpy.end() );
			if (names.size() != cpy.size()) {
				THROW( Input::ExcInputMessage() << EI_Message("The field " + this->input_name()
													+ " has set non-unique names of components.") );
			}
			shared_->n_comp_ = names.size();
    	} else {
            shared_->n_comp_ = (shared_->n_comp_ ? names.size() : 0);
    	}

    	shared_->comp_names_ = names;
    }


    /**
     * Set internal mesh pointer.
     */
    virtual void set_mesh(const Mesh &mesh) = 0;
    /**
     * Set the data list from which field will read its input. It is list of "field descriptors".
     * When reading from the input list we consider only field descriptors containing key of
     * named by the field name. These field descriptors has to have times forming ascending sequence.
     *
     * The list is used by set_time method to set field on individual regions to actual FieldBase descendants.
     */
    virtual void set_input_list(const Input::Array &list, const TimeGovernor &tg) =0;

    /**
     * Getters.
     */
    const std::string &input_name() const
    { return shared_->input_name_;}

    const std::string &name() const
    { return name_;}

    const std::string description() const
    {return shared_->input_description_;}

    const std::string &input_default() const
    { return shared_->input_default_;}

    const UnitSI &units() const
    {
        ASSERT(shared_->units_.is_def())(name()).error("Getting undefined unit.\n");
        return shared_->units_;
    }

    std::pair<double, double> limits() const
    {
        return shared_->limits_;
    }

    OutputTime::DiscreteSpace get_output_type() const
    { return default_output_data_; }

    unsigned int n_comp() const
    { return shared_->comp_names_.size();}

    /**
     * Returns full name of subfield on \p i_comp position created from component name and field name.
     *
     * If component name is empty returns only field name.
     */
    inline std::string full_comp_name(unsigned int i_comp) const
    {
        ASSERT_LT(i_comp, shared_->comp_names_.size());
        return shared_->comp_names_[i_comp].empty() ? this->name()
                : shared_->comp_names_[i_comp] + "_" + this->name();
    }

    const Mesh * mesh() const
    { return shared_->mesh_;}

    FieldFlag::Flags &flags()
    { return flags_; }

    FieldFlag::Flags get_flags() const
    { return flags_; }

    /**
     * Returns time set by last call of set_time method.
     * Can be different for different field copies.
     */
    double time() const
    { return last_time_; }

    /**
     * Returns true if the field change algorithm for the current time set through the @p set_time method.
     * This happen for all times in the field descriptors on the input of this particular field.
     */
    bool is_jump_time() {
        return is_jump_time_;
    }
    
    /**
     * Returns number of field descriptors containing the field.
     */
    unsigned int input_list_size() const {
        return shared_->input_list_.size();
    }

    /**
     * If the field on given region @p reg exists and is of type FieldConstant<...> the method method returns true
     * otherwise it returns false.
     * Then one can call ElementAccessor<spacedim>(mesh(), reg ) to construct an ElementAccessor @p elm
     * pointing to "virtual" element on which Field::value returns constant value.
     * Unlike the Field<>::field_result method, this one provides no value, so it have common header (arguments, return type) and
     * could be part of FieldCommon and FieldSet which is useful in some applications.
     *
     * TODO:Current implementation use virtual functions and can be prohibitively slow if called for every element. If this
     * becomes necessary it is possible to incorporate such test into set_time method and in this method just return precomputed result.
     */
    virtual bool is_constant(Region reg) =0;


    /**
     * @brief Indicates special field states.
     *
     * Extension of the previous method. Return possible values from the enum @p FieldResult, see description there.
     * The initial state is @p field_none, if the field is correctly set on all regions of the @p region_set given as parameter
     * we return state @p field_other or even more particular result.
     *
     * Special field values spatially constant. Could allow optimization of tensor multiplication and
     * tensor or vector addition. field_result_ should be set in constructor and in set_time method of particular Field implementation.
     * We return value @p result_none, if the field is not initialized on the region of the given element accessor @p elm.
     * Other possible results are: result_zeros, result_eye, result_ones, result_constant, result_other
     * see @p FieldResult for explanation.
     *
     * Multifield return most particular value that holds for all its subfields.
     *
     *
     */
    virtual FieldResult field_result( RegionSet region_set) const =0;

    /**
     * Return specification of the field value type in form of the string:
     * [ <element type>, NRows, NCols]
     *
     * Result is valid JSON (and/or flow style YAML).
     * For multifields not implemented.
     */
    virtual std::string get_value_attribute() const =0;

    /**
     * Returns true if set_time_result_ is not @p TimeStatus::constant.
     * Returns the same value as last set_time method.
     */
    bool changed() const
    {
    	ASSERT( set_time_result_ != TimeStatus::unknown ).error("Invalid time status.");
        return ( (set_time_result_ == TimeStatus::changed) ||
                 (set_time_result_ == TimeStatus::changed_forced) );
    }

    /**
     * Common part of the field descriptor. To get finished record
     * one has to add keys for individual fields. This is done automatically
     * using FieldSet::get_input_type().
     */
    static IT::Record field_descriptor_record(const string& record_name);

    /**
     * Create description of field descriptor record.
     */
    static const std::string field_descriptor_record_description(const string& record_name);

    /**
     * Returns input type for particular field instance, this is reference to a static member input_type of the corresponding @p FieldBase
     * class (i.e. with the same template parameters). This is used in FieldSet::make_field_descriptor_type.
     */
    virtual IT::Instance get_input_type() =0;

    /**
     * Returns input type for MultiField instance.
     * TODO: temporary solution, see @p multifield_
     */
    virtual IT::Array get_multifield_input_type() =0;

    /**
     * Pass through the input array @p input_list_, collect all times where the field could change and
     * put appropriate time marks into global TimeMarks object.
     * Introduced time marks have both given @p mark_type and @p type_input() type.
     *
     * Further development:
     * - we have to distinguish "jump" times and "smooth" times
     */
    void mark_input_times(const TimeGovernor &tg);

    /**
     * Abstract method to update field to the new time level.
     * Implemented by in class template Field<...>.
     *
     * Return true if the value of the field was changed on some region.
     * The returned value is also stored in @p changed_during_set_time data member.
     *
     * Default values helps when creating steady field. Note that default TimeGovernor constructor
     * set time to 0.0.
     *
     * Different field copies can be set to different times.
     *
     * TODO: update following:
     * Set side of limit when calling @p set_time
     * with jump time, i.e. time where the field change implementation on some region.
     * Wee assume that implementations prescribe only smooth fields.
     * This method invalidate result of
     * @p changed() so it should be called just before @p set_time.
     * Can be different for different field copies.
     */
    virtual  bool set_time(const TimeStep &time, LimitSide limit_side) =0;

    /**
     * Check that @p other is instance of the same Field<..> class and
     * perform assignment. Polymorphic copy.
     *
     * The copy is performed only if *this have set flag 'input_copy'.
     * If *this have set also the flag 'decare_input' the copy is performed only if the
     * input_list is empty.
     */
    virtual void copy_from(const FieldCommon & other) =0;

    /**
     * Output the field.
     * The parameter @p output_fields is checked for value named by the field name. If the key exists,
     * then the output of the field is performed. If the key do not appear in the input, no output is done.
     */
    virtual void field_output(std::shared_ptr<OutputTime> stream, OutputTime::DiscreteSpace type) =0;

    /**
     * Set reference of FieldSet to all instances of FieldFormula.
     */
    virtual std::vector<const FieldCommon *> set_dependency(unsigned int i_reg) const =0;

    /**
     * Sets @p component_index_
     */
    void set_component_index(unsigned int idx)
    {
    	this->component_index_ = idx;
    }

    /**
     * Return @p multifield_ flag.
     * TODO: temporary solution
     */
    inline bool is_multifield() const
    {
    	return this->multifield_;
    }

    /**
     * Reallocate field value cache of Field on given region.
     */
    virtual void cache_reallocate(const ElementCacheMap &cache_map, unsigned int region_idx) const = 0;

    /**
     * Read data to cache for appropriate elements given by ElementCacheMap object.
     */
    virtual void cache_update(ElementCacheMap &cache_map, unsigned int region_patch_idx) const = 0;


    /**
     *  Returns pointer to this (Field) or the sub-field component (MultiField).
     */
    virtual FieldCommon *get_component(FMT_UNUSED unsigned int idx) {
        return this;
    }


    /**
     *  Returns FieldValueCache if element_type of field is double or nullptr for other element_types.
     */
    virtual FieldValueCache<double> * value_cache() =0;


    /**
     * Same as previous but return const pointer
     */
    virtual const FieldValueCache<double> * value_cache() const =0;

    /// Create and set shared_ptr to ElementDataCache. Used only in descendant Field<>.
    virtual void set_output_data_cache(FMT_UNUSED OutputTime::DiscreteSpace space_type, FMT_UNUSED std::shared_ptr<OutputTime> stream)
    {
        ASSERT_PERMANENT(false);
    }

    /// Fill data to ElementDataCache on given patch.
    virtual void fill_data_value(FMT_UNUSED const std::vector<int> &offsets)
    {
        ASSERT_PERMANENT(false);
    }


    /// Fill data to ElementDataCache on given patch.
    virtual void fill_observe_value(FMT_UNUSED std::shared_ptr<ElementDataCacheBase> output_cache_base, FMT_UNUSED const std::vector<int> &offsets)
    {
        ASSERT_PERMANENT(false);
    }


    /**
     * Print stored messages to table.
     *
     * Return true if messages_data_ vector is nonempty and clear its.
     */
    static bool print_message_table(ostream& stream, std::string equation_name);

    /**
     * Setter of @p shared_->default_fieldset_
     *
     * Sets FieldSet which is default owner of Field. This FieldSet is used during evaluation
     * of user field in FieldFormula.
     */
    inline void set_default_fieldset(FieldSet &default_fs) {
        shared_->default_fieldset_ = &default_fs;
    }

    /**
     * Virtual destructor.
     */
    virtual ~FieldCommon();

    /// Return number of shape components
    inline uint n_shape() const {
        if (shape_.size() == 1) return shape_[0];
        else return shape_[0] * shape_[1];
    }

    /**
     * Hold shape of Field.
     *
     * Value is set in constructor of descendant class.
     */
    std::vector<uint> shape_;


protected:
    /**
     * Private default constructor. Should be used only through
     * Field<...>
     */
    FieldCommon();

    /**
     * Private copy constructor. Should be used only through
     * Field<...>
     */
    FieldCommon(const FieldCommon & other);

    /**
     * Invalidate last time in order to force set_time method
     * update region_fields_.
     */
    void set_history_changed()
    {
        last_time_ = -numeric_limits<double>::infinity();
    }

    void set_shape(uint n_rows, uint n_cols) {
        if (n_cols==1) this->shape_ = { n_rows };
        else this->shape_ = { n_rows, n_cols };
    }

    /**
     * Setters for essential field properties.
     */
    /**
     *  Data shared among copies of the same field.
     *
     *  This allow field copies in different equations with different time setting, but
     *  sharing common input field descriptor array and common history.
     */
    struct SharedData {
    	/**
    	 * Empty constructor.
    	 */
    	SharedData()
    	: list_idx_(0), limits_(std::make_pair(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max())) {};

        /**
         * Number of components for fields that return variable size vectors. Zero in other cases.
         */
        unsigned int n_comp_;
        /**
         * Names of field components.
         */
        std::vector< std::string > comp_names_;
        /**
         * Name of the particular field. Used to name the key in the Field list Record.
         */
        std::string input_name_;
        /**
         * Description of corresponding key in the Field list Record.
         */
        std::string input_description_;
        /**
         * Units of the field values. Currently just a string description.
         */
        UnitSI units_;
        /**
         * For Enum valued fields this is the input type selection that should be used
         * to read possible values of the field (e.g. for FieldConstant the key 'value' has this selection input type).
         *
         * Is empty selection for for non-enum values fields.
         */
        IT::Selection input_element_selection_;
        /**
         * Possible default value of the field.
         */
        string input_default_;
        /**
         * Pointer to the mesh on which the field lives.
         */
        const Mesh *mesh_;

        /**
         * Vector of input field descriptors from which the field is set.
         */
        vector<Input::Record> input_list_;

        /**
         * Index to current position of input field descriptor.
         */
        unsigned int list_idx_;

        /**
         * True after check_initialized_region_fields_ is called. That happen at first call of the set_time method.
         */
        bool is_fully_initialized_;

        /**
         * For which values of an enum valued field we do not
         * check the field. User is responsible, that the value will not be called
         * on such regions.
         */
        std::vector<FieldEnum> no_check_values_;

        /**
         * Allow set minimal and maximal limit value of Field.
         */
        std::pair<double, double> limits_;

        /**
         * Holds FieldSet which is default owner of Field.
         *
         * This data member is set in method FieldSet::make_field_descriptor_type and it is used during evaluation
         * of user field in FieldFormula.
         */
        FieldSet *default_fieldset_;


    };

    /**
     * Name that identifies the field in the field_set. By default this is same as
     * shared_->input_name_.
     */
    std::string name_;

    /**
     * Data shared among copies of the same input field.
     */
    std::shared_ptr<SharedData> shared_;

    /**
     * Result of last set time method
     */
    enum class TimeStatus {
        changed,    //<  Field changed during last set time call.
        constant,   //<  Field doesn't change.
        changed_forced, //< Field changed manually (reset during set_time() to changed).
        unknown     //<  Before first call of set_time.
    };

    // TODO: Merge time information: set_time_result_, last_time_, last_limit_side_, is_jump_time into
    // a single structure with single getter.
    /**
     * Status of @p history.
     */
    TimeStatus set_time_result_;

    /**
     * Last set time. Can be different for different field copies.
     * Store also time limit, since the field may be discontinuous.
     */
    double last_time_ = -numeric_limits<double>::infinity();
    LimitSide last_limit_side_ = LimitSide::left;

    /**
     * Set to true by the @p set_time method the field algorithm change on any region.
     * Accessible through the @p is_jump_time method.
     */
    bool is_jump_time_;

    /**
     * Default output data type used in the output() method. Can be different for different field copies.
     */
    OutputTime::DiscreteSpace default_output_data_ = OutputTime::ELEM_DATA;

    /**
     * Specify if the field is part of a MultiField and which component it is
     */
    unsigned int component_index_;

    /**
     * Flag determining if object is Multifield or Field.
     * TODO: temporary solution, goal is to make these two classes to behave similarly
     */
    bool multifield_;

    /**
     * Maximum number of FieldBase objects we store per one region.
     */
    static const unsigned int history_length_limit_=3;

    /// Field flags. Default setting is "an equation input field, that can read from user input, and can be written to output"
    FieldFlag::Flags   flags_ = FieldFlag::declare_input & FieldFlag::equation_input & FieldFlag::allow_output;

	/// Vector of data of initialization messages.
	static std::vector<MessageData> messages_data_;

	/**
     * Stream output operator
     */
    friend std::ostream &operator<<(std::ostream &stream, const FieldCommon &field) {

        vector<string> limit_side_str = {"left", "right"};

        stream
        << "field name:" << field.name()
        << " n. comp.:" << field.n_comp()
        << " last time:" << field.last_time_
        << " last limit side:" << limit_side_str[(unsigned int) field.last_limit_side_];
        return stream;
    }
    
public:
    
    /// Manually mark flag that the field has been changed.
    void set_time_result_changed()
    { set_time_result_ = TimeStatus::changed_forced; }
};







#endif /* FIELD_COMMON_HH_ */
