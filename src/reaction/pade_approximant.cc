#include "reaction/reaction.hh"
#include "reaction/linear_reaction.hh"
#include "reaction/pade_approximant.hh"
#include "system/global_defs.h"

// #include "la/distribution.hh"
// #include "mesh/mesh.h"
#include "armadillo"

using namespace arma;
using namespace std;
using namespace Input::Type;

Record PadeApproximant::input_type_one_decay_substep
	= Record("Substep", "Equation for reading information about radioactive decays.")
	.declare_key("parent", String(), Default::obligatory(),
				"Identifier of an isotope.")
    .declare_key("half_life", Double(), Default::optional(),
                "Half life of the parent substance.")
    .declare_key("kinetic", Double(), Default::optional(),
                "Kinetic constants describing first order reactions.")
	.declare_key("products", Array(String()), Default::obligatory(),
				"Identifies isotopes which decays parental atom to.")
	.declare_key("branch_ratios", Array(Double()), Default("1.0"),  //default is one product, with ratio = 1.0
				"Decay chain branching percentage.");


Record PadeApproximant::input_type
	= Record("PadeApproximant", "Abstract record with an information about pade approximant parameters.")
	.derive_from( ReactionTerm::input_type )
    .declare_key("decays", Array( PadeApproximant::input_type_one_decay_substep ), Default::obligatory(),
                "Description of particular decay chain substeps.")
	.declare_key("nom_pol_deg", Integer(), Default("2"),
				"Polynomial degree of the nominator of Pade approximant.")
	.declare_key("den_pol_deg", Integer(), Default("2"),
				"Polynomial degree of the nominator of Pade approximant");


PadeApproximant::PadeApproximant(Mesh &init_mesh, Input::Record in_rec)
      : LinearReaction(init_mesh, in_rec)
{
}

PadeApproximant::~PadeApproximant()
{
}

void PadeApproximant::initialize()
{
    LinearReaction::initialize();
    
    // init from input
    nom_pol_deg = input_record_.val<int>("nom_pol_deg");
    den_pol_deg = input_record_.val<int>("den_pol_deg");
    if((nom_pol_deg + den_pol_deg) < 0){
        xprintf(UsrErr, "You did not specify Pade approximant required polynomial degrees.");
    }
}

void PadeApproximant::zero_time_step()
{
    LinearReaction::zero_time_step();
}

void PadeApproximant::modify_reaction_matrix(void )
{
    // create decay matrix
    mat r_reaction_matrix_ = zeros(n_substances_, n_substances_);
    unsigned int reactant_index, product_index; //global indices of the substances
    double exponent;    //temporary variable
    for (unsigned int i_decay = 0; i_decay < half_lives_.size(); i_decay++) {
        reactant_index = substance_ids_[i_decay][0];
        exponent = log(2) * time_->dt() / half_lives_[i_decay];
        r_reaction_matrix_(reactant_index, reactant_index) = -exponent;
        
        for (unsigned int i_product = 1; i_product < substance_ids_[i_decay].size(); ++i_product){
            product_index = substance_ids_[i_decay][i_product];
            r_reaction_matrix_(reactant_index, product_index) = exponent * bifurcation_[i_decay][i_product-1];
        }
    }
    //DBGMSG("reactions_matrix_created\n");
    //r_reaction_matrix_.print();
    
    //compute Pade Approximant
    mat nominator_matrix(n_substances_, n_substances_),
        denominator_matrix(n_substances_, n_substances_),
        pade_approximant_matrix(n_substances_, n_substances_);
        
    nominator_matrix.fill(0);
    denominator_matrix.fill(0);
    pade_approximant_matrix.fill(0);

    std::vector<double> nominator_coefs(nom_pol_deg+1),
                        denominator_coefs(den_pol_deg+1);
    
    // compute Pade approximant polynomials for the function e^x
    compute_exp_coefs(nom_pol_deg, den_pol_deg, nominator_coefs, denominator_coefs);  
    // evaluation of polynomials of Pade approximant where x = -kt = R
    evaluate_matrix_polynomial(nominator_matrix, r_reaction_matrix_, nominator_coefs);
    evaluate_matrix_polynomial(denominator_matrix, r_reaction_matrix_, denominator_coefs);
    // compute P(R(t)) / Q(R(t))
    pade_approximant_matrix = nominator_matrix * inv(denominator_matrix);
    //pade_approximant_matrix.print();
    
    // write matrix to reaction matrix
    unsigned int rows, cols;
    for(rows = 0; rows < n_substances_; rows++)
    {
        for(cols = 0; cols < n_substances_ ; cols++)
        {
            reaction_matrix_[rows][cols] = pade_approximant_matrix(cols,rows);
        }
    }
    //print_reaction_matrix();
}

void PadeApproximant::compute_exp_coefs(unsigned int nominator_degree, 
                                        unsigned int denominator_degree, 
                                        std::vector< double >& nominator_coefs, 
                                        std::vector< double >& denominator_coefs)
{
    //compute some of the factorials forward
    unsigned int nom_fact = factorial(nominator_degree),
                 den_fact = factorial(denominator_degree),
                 nom_den_fact = factorial(nominator_degree + denominator_degree);
    int sign;   // variable for denominator sign alternation
    
    for(int j = nominator_degree; j >= 0; j--)
    {
        nominator_coefs[j] = (double)(factorial(nom_pol_deg + den_pol_deg - j) * nom_fact) 
                             / (nom_den_fact * factorial(j) * factorial(nom_pol_deg - j));
        //DBGMSG("p(%d)=%f\n",j,nominator_coefs[j]);
    }

    for(int i = denominator_degree; i >= 0; i--)
    {
        if(i % 2 == 0) sign = 1; else sign = -1;
        denominator_coefs[i] = sign * (double)(factorial(nom_pol_deg + den_pol_deg - i) * den_fact)
                               / (nom_den_fact * factorial(i) * factorial(den_pol_deg - i));
        //DBGMSG("q(%d)=%f\n",i,denominator_coefs[i]);
    } 
}

void PadeApproximant::evaluate_matrix_polynomial(mat& polynomial_matrix, 
                                                 const mat& reaction_matrix, 
                                                 const std::vector< double >& coefs)
{
    //DBGMSG("evaluate_matrix_polynomial\n");
    mat identity = eye(n_substances_, n_substances_);

    ///Horner scheme for evaluating polynomial a0 + R(t)[a1 + R(t)[a2 + R(t)[a3 +...]]]
    for(int i = coefs.size()-1; i >= 0; i--)
    {
        polynomial_matrix = coefs[i] * identity + (polynomial_matrix * reaction_matrix);
    }
    //polynomial_matrix.print();
}

/*
void PadeApproximant::modify_reaction_matrix(void)
{   
	Mat Denominator;
	Mat Nominator;
	Mat Pade_approximant;
	//MatFactorInfo matfact;
	PC Precond;
	//IS rperm, cperm;
	Vec tmp1; //contains the information about concentrations of all the species in one particular element
	Vec tmp2; //the same as tmp1
	//PetscInt n, m = 2;
	PetscScalar nominator_coef[nom_pol_deg];
	PetscScalar denominator_coef[den_pol_deg];
	PetscScalar Hlp_mat[1];
	PetscScalar *Array_hlp;
	//const PetscScalar *Reaction_matrix_row;
	//char dec_name[30];
	int rows, cols, i, j; //int dec_nr, dec_name_nr = 1, index, prev_index;

	//create the matrix Reaction_matrix
	MatCreate(PETSC_COMM_SELF, &Reaction_matrix);
    
    //should be probably multiplied by 2 (which is the value of m)
	MatSetSizes(Reaction_matrix, PETSC_DECIDE, PETSC_DECIDE, n_substances_, n_substances_); 
	MatSetType(Reaction_matrix, MATAIJ);
	MatSetUp(Reaction_matrix);


	//It is necessery to initialize reaction matrix here
	int index_par;
	int index_child;
	PetscScalar rel_step;
	PetscScalar extent;
    for (unsigned int i_decay = 0; i_decay < half_lives_.size(); i_decay++) {
        index_par = substance_ids_[i_decay][0];
        rel_step = time_->dt() / half_lives_[i_decay];
        //DBGMSG("time_dt: %f, half_life: %f rel_step: %f\n", time_->dt(), half_lives_[i_decay], rel_step);
        extent = -log(2)*rel_step; //pow(0.5, rel_step);
        //PetscPrintf(PETSC_COMM_WORLD,"extent %f\n", PetscRealPart(extent));
        MatSetValue(Reaction_matrix, index_par, index_par, extent,INSERT_VALUES);
        for (unsigned int i_product = 1; i_product < substance_ids_[i_decay].size(); ++i_product){
            extent = log(2)*rel_step* bifurcation_[i_decay][i_product-1];
            index_child = substance_ids_[i_decay][i_product];
        	MatSetValue(Reaction_matrix, index_par, index_child,extent,INSERT_VALUES);
        }
    }

	MatAssemblyBegin(Reaction_matrix, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(Reaction_matrix, MAT_FINAL_ASSEMBLY);

	//create the matrix N
    MatDuplicate(Reaction_matrix, MAT_DO_NOT_COPY_VALUES, &Nominator);

    //create the matrix D
    MatDuplicate(Reaction_matrix, MAT_DO_NOT_COPY_VALUES, &Denominator);


	//Computation of nominator in pade approximant follows
	MatZeroEntries(Nominator);
	//MatAssemblyBegin(Nominator, MAT_FINAL_ASSEMBLY);
	//MatAssemblyEnd(Nominator, MAT_FINAL_ASSEMBLY);
	for(j = nom_pol_deg; j >= 0; j--)
	{
		nominator_coef[j] = (PetscScalar) (factorial(nom_pol_deg + den_pol_deg - j) * factorial(nom_pol_deg)) 
                        / (factorial(nom_pol_deg + den_pol_deg) * factorial(j) * factorial(nom_pol_deg - j));
	}
	evaluate_matrix_polynomial(&Nominator, &Reaction_matrix, nominator_coef);
	//MatView(Nominator,PETSC_VIEWER_STDOUT_WORLD);

	//Computation of denominator in pade approximant follows
	MatZeroEntries(Denominator);
	//MatAssemblyBegin(Denominator, MAT_FINAL_ASSEMBLY);
	//MatAssemblyEnd(Denominator, MAT_FINAL_ASSEMBLY);
	for(i = den_pol_deg; i >= 0; i--)
	{
		denominator_coef[i] = (PetscScalar) pow(-1.0,i) * factorial(nom_pol_deg + den_pol_deg - i) 
                              * factorial(den_pol_deg) / (factorial(nom_pol_deg + den_pol_deg) 
                              * factorial(i) * factorial(den_pol_deg - i));
	}
	evaluate_matrix_polynomial(&Denominator, &Reaction_matrix, denominator_coef);
	//MatView(Denominator, PETSC_VIEWER_STDOUT_WORLD);



	PCCreate(PETSC_COMM_WORLD, &Precond);
	PCSetType(Precond, PCLU);
	PCSetOperators(Precond, Denominator, Denominator, DIFFERENT_NONZERO_PATTERN);
	//PCFactorSetMatOrderingType(Precond, MATORDERINGNATURAL);
	PCFactorSetMatOrderingType(Precond, MATORDERINGRCM);
	PCSetUp(Precond);

	VecCreate(PETSC_COMM_WORLD, &tmp1);
	VecSetSizes(tmp1, PETSC_DECIDE, n_substances_);
	VecSetFromOptions(tmp1);
	VecDuplicate(tmp1, &tmp2);


    //create the matrix pade
    MatCreate(PETSC_COMM_SELF, &Pade_approximant);
    
    //should be probably multiplied by 2 (which is the value of m)
    MatSetSizes(Pade_approximant, PETSC_DECIDE, PETSC_DECIDE, n_substances_, n_substances_);
    MatSetType(Pade_approximant, MATAIJ);
    MatSetUp(Pade_approximant);

	for(rows = 0; rows < n_substances_ ; rows++){
        DBGMSG("error\n");
		MatGetColumnVector(Nominator, tmp1, rows);
		//VecView(tmp1, PETSC_VIEWER_STDOUT_SELF);
		PCApply(Precond, tmp1, tmp2);
        DBGMSG("error\n");
		PCView(Precond, PETSC_VIEWER_STDOUT_WORLD);
		//VecView(tmp2, PETSC_VIEWER_STDOUT_SELF);
		VecGetArray(tmp2, &Array_hlp);
		for(cols = 0; cols < n_substances_; cols++)
		{
			MatSetValue(Pade_approximant, rows, cols, Array_hlp[cols], ADD_VALUES);
		}
	}
	MatAssemblyBegin(Pade_approximant, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(Pade_approximant, MAT_FINAL_ASSEMBLY);

	//pade assembled to reaction_matrix
	for(rows = 0; rows < n_substances_; rows++)
		{
			for(cols = 0; cols < n_substances_; cols++)
			{
				reaction_matrix_[rows][cols] = 0.0;
			}
		}
	for(rows = 0; rows < n_substances_; rows++)
	{
		for(cols = 0; cols < n_substances_ ; cols++)
		{
			MatGetValues(Pade_approximant, 1, &rows, 1, &cols, Hlp_mat); //&Hlp_mat[n_substances_*rows + cols]);
			reaction_matrix_[rows][cols] = (double) (Hlp_mat[0]);
		}
	}

	print_reaction_matrix(); //for visual control of equality of reaction_matrix in comparison with pade aproximant

	VecDestroy(&tmp1);
	VecDestroy(&tmp2);
	PCDestroy(&Precond);
	MatDestroy(&Denominator);
	MatDestroy(&Nominator);
	MatDestroy(&Pade_approximant);
}
    //*/
 /*
void PadeApproximant::evaluate_matrix_polynomial(Mat *Polynomial, Mat *Reaction_matrix, PetscScalar *coef)
{
   
	Mat Identity;

	//create Identity matrix
	MatCreate(PETSC_COMM_SELF, &Identity);
	MatSetSizes(Identity, PETSC_DECIDE, PETSC_DECIDE, n_substances_, n_substances_); //should be probably multiplied by 2 (which is the value of m)
	MatSetType(Identity, MATAIJ);
	MatSetUp(Identity);

	MatAssemblyBegin(Identity, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(Identity, MAT_FINAL_ASSEMBLY);
	MatShift(Identity, 1.0);

	for(int i = den_pol_deg; i >= 0; i--)
		{
            //Performs Matrix-Matrix Multiplication C=A*B.
            //PetscErrorCode  MatMatMult(Mat A,Mat B,MatReuse scall,PetscReal fill,Mat *C)
			MatMatMult(*Polynomial, *Reaction_matrix, MAT_INITIAL_MATRIX, PETSC_DEFAULT, Polynomial);
            //Computes Y = a*X + Y.
            //PetscErrorCode  MatAXPY(Mat Y,PetscScalar a,Mat X,MatStructure str)
			MatAXPY(*Polynomial, coef[i], Identity, DIFFERENT_NONZERO_PATTERN);
		}

	MatDestroy(&Identity);
}
//*/

unsigned int PadeApproximant::factorial(int k)
{
    ASSERT(k >= 0, "Cannot compute factorial of negative number.");
    
    unsigned int fact = 1;
    while(k > 1)
    {
            fact *= k;
            k--;
    }
    return fact;
}
