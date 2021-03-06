/*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* This program was written while working at Bright Works Computer Consulting
* and allowed to be GPL'd under express permission of the current president
* John Guttridge
* Dated May 20th 2013
*/

#include "gbl2ngc.hpp"


class Gerber_point_2 {
  public:
    int ix, iy;
    double x, y;
    int jump_pos;
};

class irange_2 {
  public:
    int start, end;
};

typedef struct dpoint_2_type {
  int ix, iy;
  double x, y;
} dpoint_2_t;

struct Gerber_point_2_cmp
{
  bool operator()(const Gerber_point_2 &lhs, const Gerber_point_2 &rhs)
  {
    if (lhs.ix < rhs.ix) return true;
    if (lhs.ix > rhs.ix) return false;
    if (lhs.iy < rhs.iy) return true;
    return false;
  }
};

typedef std::map< Gerber_point_2, int, Gerber_point_2_cmp  > PointPosMap;
typedef std::pair< Gerber_point_2, int > PointPosMapPair;

typedef std::map< Gerber_point_2, int , Gerber_point_2_cmp  > HolePosMap;



// add a hole to the vector hole_vec from the Gerber_point_2
// vector p.  The points are decorated with a 'jump_pos' which
// holds the position of the first time the path comes back
// to the same point.  The path can come back to the same
// point more than twice, so we need to skip over the duplicate
// points.
//
// Here is some ASCII art to illustrate:
//
//            >...     ...>
//                \   /
//                 a a
//                 | |
//                 | |
//            <----b b-----<
//
// where a is a distinct point from b and the 'a' points overlap
// and the 'b' points overlap.  The hole really starts (and ends)
// at point 'a'.  The outer region connects to the inner hole
// through point 'b'.
// start and end hold the positions the starting 'a' and ending 'a'
// point in the p vector.
//
void add_hole( Paths &hole_vec,
               std::vector< Gerber_point_2 > &p,
               int start,
               int n )
{
  int i, ds, jp, end;
  Path hole_polygon;

  end = start+n;

  jp = p[start].jump_pos;

  if (jp<0) {
    fprintf(stderr, "ERROR: jump pos MUST be > 0, is %i, exiting (start %i, n %i)\n", jp, start, n );
    printf("ERROR: jump pos MUST be > 0, is %i, exiting (start %i, n %i)\n", jp, start, n );
    exit(2);
  }

  while ( p[jp].jump_pos >= 0 )
  {
    ds = p[start].jump_pos - (start+1);
    add_hole( hole_vec, p, start+1, ds );
    start = jp;
    jp = p[start].jump_pos;
  }

  n = end-start;
  hole_polygon.push_back( dtoc(p[start].x, p[start].y) );

  for (i=start+1; i<(start+n); i++) 
  {
    hole_polygon.push_back( dtoc(p[i].x, p[i].y) );
    if (p[i].jump_pos<0) continue;

    ds = p[i].jump_pos - (i+1);
    add_hole( hole_vec, p, i+1, ds);
    i = p[i].jump_pos;

  }

  if (hole_polygon.size() > 2)
    hole_vec.push_back( hole_polygon );

}



// Copy linked list points in contour to vector p, removing contiguougs
// duplicates.
//
void populate_gerber_point_vector_from_contour( std::vector< Gerber_point_2 > &p,
                                                contour_ll_t *contour)
{
  int first=1;
  Gerber_point_2 prev_pnt;
  Gerber_point_2 dpnt;

  // Get rid of points that are duplicated next to each other.
  // It does no good to have them and it just makes things more
  // complicated downstream to work around them.
  //
  while (contour)
  {
    dpnt.ix = int(contour->x * 10000.0);
    dpnt.iy = int(contour->y * 10000.0);
    dpnt.x = contour->x;
    dpnt.y = contour->y;

    if (first) {
      p.push_back( dpnt );
    }
    else if ( ( prev_pnt.ix == dpnt.ix ) 
           && ( prev_pnt.iy == dpnt.iy ) ) 
    {
      // duplicate
    }
    else
    {
      p.push_back( dpnt );
    }

    prev_pnt = dpnt;
    first = 0;

    contour = contour->next;
  }

}


// Create the start and end positions of the holes in the vector p.
// Store in hole_map.
//
int gerber_point_2_decorate_with_jump_pos( std::vector< Gerber_point_2 > &p )
{

  int i, j, k;
  irange_2 range;
  HolePosMap hole_map;

  for (i=0; i<p.size(); i++)
  {
    p[i].jump_pos = -1;

    // if found in p_map, add it to our map
    //
    if ( hole_map.find( p[i] ) != hole_map.end() )
    {
      k = hole_map[ p[i] ];
      p[ k ].jump_pos = i;

      hole_map[ p[i] ] = i;
    }
    else   // otherwise add it
    {
      hole_map[ p[i] ] = i;
    }

  }

  return 0;

}


//----------------------------------

// Very similar to add_hole but special care needs to be taken for the outer
// contour.
// Construct the point vector and hole map.  Create the polygon with holes
// vector.
//
int construct_contour_region( PathSet &pwh_vec, contour_ll_t *contour )
{
  int i, j, k;
  int jp, ds;
  int start=0;

  std::vector< Gerber_point_2 > p;
  Paths hole_vec;

  Path outer_boundary_polygon;
  Paths pwh;

  // Initially populate p vector
  //
  populate_gerber_point_vector_from_contour( p, contour );

  // Find the start and end regions for each of the
  // holes.
  gerber_point_2_decorate_with_jump_pos( p );

  if (p.size()==0) return -1;

  int n = p.size();
  for (i=0; i<n; i++) 
  {

    outer_boundary_polygon.push_back( dtoc( p[i].x, p[i].y ) );
    if (p[i].jump_pos < 0) continue;

    // Special case when the boundary end ties back to the beginning
    // without any jumps to holes.
    //
    if (p[i].jump_pos == (n-1)) continue;

    ds = p[i].jump_pos - (i+1);
    add_hole( hole_vec, p, i+1, ds );
    i += ds;

  }

  // Make sure orientation is correct for outer boundary and holes inside.
  //
  if ( Area(outer_boundary_polygon) < 0.0 )
    std::reverse( outer_boundary_polygon.begin(), outer_boundary_polygon.end() );
  pwh.push_back( outer_boundary_polygon ); 

  for (i=0; i<hole_vec.size(); i++)
  {
    if ( Area(hole_vec[i]) > 0.0)
      std::reverse( hole_vec[i].begin(), hole_vec[i].end() );
    pwh.push_back( hole_vec[i] );
  }
  pwh_vec.push_back( pwh );

}


//-----------------------------------------

bool isccw( Path &p )
{
  int i, j, k;
  long double a, b, c;
  cInt dx, dy, s = 0;


  for (i=1; i<p.size(); i++)
  {
    dx = p[i].X - p[0].X;
    dy = p[i].Y - p[0].Y;
    s += dx*dy;
  }

  if (s < 0) return true;
  return false;
}

// construct the final polygon set (before offsetting)
//   of the joined polygons from the parsed gerber file.
//
void join_polygon_set(Paths &result, gerber_state_t *gs)
{

  int i, j, k;
  int count = 0;
  contour_list_ll_t *contour_list;
  contour_ll_t      *contour, *prev_contour;

  PathSet temp_pwh_vec;
  IntPoint prev_pnt, cur_pnt;
  Clipper clip;


  for (contour_list = gs->contour_list_head;
       contour_list;
       contour_list = contour_list->next)
  {
    prev_contour = NULL;
    contour = contour_list->c;

    if (contour->region)
    {
      construct_contour_region(temp_pwh_vec, contour);
      continue;
    }


    while (contour)
    {
      if (!prev_contour)
      {
        prev_contour = contour;
        continue;
      }

      Path point_list;
      Path res_point;
      int name = contour->d_name;

      prev_pnt = dtoc( prev_contour->x, prev_contour->y );
      cur_pnt = dtoc( contour->x, contour->y );

      // An informative example:
      // Consider drawing a line with rounded corners from position prev_contour->[xy] to contour->[xy].
      // This is done by first drawing a (linearized) circle (in general, an aperture) at prev_countour,
      // then drawing another circle at countour.  After those two are formed, take the convex hull
      // of the two (this is the ch_graham_andrew call).  Now we have a "line" with width, which is
      // really a polygon that we've constructed.
      // After all these polygoin segments have been constructed, do a big join at the end to eliminate
      // overlap.
      //
      for (i=0; i<gAperture[ name ].m_outer_boundary.size(); i++)
        point_list.push_back( IntPoint(  gAperture[ name ].m_outer_boundary[i].X + prev_pnt.X,
                                         gAperture[ name ].m_outer_boundary[i].Y + prev_pnt.Y  ) );

      for (i=0; i<gAperture[ name ].m_outer_boundary.size(); i++)
        point_list.push_back( IntPoint( gAperture[ name ].m_outer_boundary[i].X + cur_pnt.X ,
                                        gAperture[ name ].m_outer_boundary[i].Y + cur_pnt.Y ) );

      ConvexHull( point_list, res_point );

      if (res_point[ res_point.size() - 1] == res_point[0])
      {
        res_point.pop_back();
      }

      if (Area(res_point) < 0.0)
      {
        std::reverse( res_point.begin(), res_point.end() );
      }

      clip.AddPath( res_point, ptSubject, true );
      contour = contour->next;
    }

  }

  for (i=0; i<temp_pwh_vec.size(); i++)
  {
    clip.AddPaths( temp_pwh_vec[i], ptSubject, true );
  }

  clip.Execute( ctUnion, result, pftNonZero, pftNonZero  );
}
