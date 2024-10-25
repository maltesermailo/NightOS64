//
// Created by Jannik on 25.10.2024.
//
static int last_error;

int get_last_error() {
  return last_error;
}

void set_last_error(int error) {
  last_error = error;
}

inline void clear_error() {
  last_error = 0;
}

inline int clear_and_report_error() {
  int error = last_error;

  clear_error();

  return error;
}