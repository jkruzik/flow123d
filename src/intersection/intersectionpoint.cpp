/*
 * intersectionpoint.cpp
 *
 *  Created on: 11.4.2014
 *      Author: viktor
 */

#include "intersectionpoint.h"

namespace computeintersection{

template<> bool IntersectionPoint<2,3>::operator<(const IntersectionPoint<2,3> &ip) const{
	return local_coords1[1] < ip.getLC1()[1] ||
		(local_coords1[1] == ip.getLC1()[1] &&
		 local_coords1[2] < ip.getLC1()[2]);
};

} // END namespace



