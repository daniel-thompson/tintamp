/*
 * rand.h
 *
 * Part of librfn (a general utility library from redfelineninja.org.uk)
 *
 * Copyright (C) 2012 Daniel Thompson <daniel@redfelineninja.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef RF_RAND_H_
#define RF_RAND_H_

#include <stdint.h>

/*!
 * Initialize a seed value used by rand31_r().
 *
 * The only actualy requirement on the initial seed is that it must be
 * strictly greater than zero (and 32-bit). However for simplicity we
 * choose 1.
 */
#define RAND31_VAR_INIT 1

uint32_t rand31_r(uint32_t *seedp);

#endif // RF_RAND_H_
