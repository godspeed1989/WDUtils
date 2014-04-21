#pragma once

void init_spin_lock (PLONG lock);

void spin_lock (PLONG lock);

void spin_unlock (PLONG lock);
