/**************************************************************************
 * This file is part of K3Match.                                          *
 * Copyright (C) 2016 Pim Schellart <P.Schellart@astro.ru.nl>             *
 *                                                                        *
 * This Source Code Form is subject to the terms of the Mozilla Public    *
 * License, v. 2.0. If a copy of the MPL was not distributed with this    *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.               *
 **************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <k3match.h>

#define SQUARE(x) ((x) * (x))

void k3m_build_balanced_tree(node_t *tree, point_t **points, int_t npoints, int axis, int_t *npool)
{
  node_t *current = tree+(*npool);

  int next_axis = (axis + 1) % 3;

  int_t nleft, nright;

  current->left = NULL;
  current->right = NULL;
  current->axis = axis;

  current->point = k3m_median(points, npoints, current->axis);

  nright = npoints / 2;
  nleft = nright - (1 - npoints % 2);

  if (nleft > 0)
  {
    (*npool)++;
    current->left = tree+(*npool);
    current->left->parent = &(*current);
    k3m_build_balanced_tree(tree, points, nleft, next_axis, npool);
  }

  if (nright > 0)
  {
    (*npool)++;
    current->right = tree+(*npool);
    current->right->parent = &(*current);
    k3m_build_balanced_tree(tree, points+nleft+1, nright, next_axis, npool);
  }
}

node_t* k3m_insert_node(node_t *tree, node_t *node)
{
    int axis = 0;
    node_t *parent = NULL;
    node_t *current = tree;

    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;

    while (current != NULL) {
        parent = current;
        if (node->point->value[axis] < current->point->value[axis]) {
            current = current->left;
        } else {
            current = current->right;
        }
        axis = (axis + 1) % 3;
    }

    node->parent = parent;
    node->axis = axis;

    if (tree == NULL) {
        tree = node;
    } else if (node->point->value[parent->axis] < parent->point->value[parent->axis]) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    return tree;
}

void k3m_print_tree(node_t *tree)
{
  if (!tree) return;

  k3m_print_tree(tree->left);
  printf("%lu %f %f %f\n", (unsigned long)tree->point->id, tree->point->value[0], tree->point->value[1], tree->point->value[2]);
  k3m_print_tree(tree->right);
}

void k3m_print_dot_tree(node_t *tree)
{
  if (!tree) return;

  if (tree->left != NULL)
  {
    printf("%lu -> %lu;\n", (unsigned long)tree->point->id, (unsigned long)tree->left->point->id);
  }
  
  if (tree->right != NULL)
  {
    printf("%lu -> %lu;\n", (unsigned long)tree->point->id, (unsigned long)tree->right->point->id);
  }

  printf("%lu [label=\"%lu\\n %f %f %f\"];\n", (unsigned long)tree->point->id, (unsigned long)tree->point->id,
      tree->point->value[0], tree->point->value[1], tree->point->value[2]);

  k3m_print_dot_tree(tree->left);
  k3m_print_dot_tree(tree->right);
}

node_t* k3m_closest_leaf(node_t *tree, point_t *point)
{
  node_t* current = tree;
  node_t* closest = NULL;

  while (current)
  {
    closest = current;

    if (point->value[current->axis] > current->point->value[current->axis])
    {
      current = current->right;
    }
    else
    {
      current = current->left;
    }
  }

  return closest;
}

node_t* k3m_nearest_neighbour(node_t *tree, point_t *point)
{
  node_t* nearest = k3m_closest_leaf(tree, point);
  node_t* current = nearest;
  node_t* sub = NULL;
  node_t* last = NULL;

  real_t dn = k3m_distance_squared(nearest->point, point);
  real_t dc = dn;
  real_t ds;

  while (1)
  {
    dc = k3m_distance_squared(current->point, point);
    if (dc < dn)
    {
      nearest = current;
      dn = dc;
    }

    if ((current->point->value[current->axis] - point->value[current->axis]) * (current->point->value[current->axis] - point->value[current->axis]) < dn)
    {
      if (last == current->left && current->right != NULL)
      {
        sub = k3m_nearest_neighbour(current->right, point);

        ds = k3m_distance_squared(sub->point, point);
        if (ds < dn)
        {
          nearest = sub;
          dn = ds;
        }
      }
      else if (last == current->right && current->left != NULL)
      {
        sub = k3m_nearest_neighbour(current->left, point);

        ds = k3m_distance_squared(sub->point, point);
        if (ds < dn)
        {
          nearest = sub;
          dn = ds;
        }
      }
    }

    if (current == tree)
    {
      break;
    }
    else
    {
      last = current;
      current = current->parent;
    }
  }

  if (nearest == NULL)
  {
    return tree;
  }
  else
  {
    return nearest;
  }
}

int_t k3m_in_range(node_t *tree, point_t **match, point_t *search, real_t ds)
{
  node_t* current = tree;
  real_t d[3] = {0, 0, 0};
  int_t nmatch = 0;
  real_t dc = 0;
  int i = 0;

  while (current)
  {
    /* calculate distance from current point to search point */
    for (i=0; i<3; i++)
    {
      d[i] = SQUARE(current->point->value[i] - search->value[i]);
    }
    dc = d[0] + d[1] + d[2];

    /* check if current point is within search radius */
    if (dc < ds)
    {
      current->point->ds = dc;
      current->point->neighbour = *match;
      *match = &(*current->point);
      nmatch++;
    }

    /* next point is on the same side of the partition plane as the search point */
    if (search->value[current->axis] > current->point->value[current->axis])
    {
      /* check if we need to examine the points on the opposite side */
      if (d[current->axis] < ds)
      {
        nmatch += k3m_in_range(current->left, match, search, ds);
      }

      current = current->right;
    }
    else
    {
      /* check if we need to examine the points on the opposite side */
      if (d[current->axis] < ds)
      {
        nmatch += k3m_in_range(current->right, match, search, ds);
      }

      current = current->left;
    }
  }

  return nmatch;
}

