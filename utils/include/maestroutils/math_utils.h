

static inline int math_get_avg_int(const int* _inputs, const int _count)
{
  if (!_inputs)
    return 0.0;

  int i;
  int counter = 0.0;
  for (i = 0; i < _count; i++)
    counter += _inputs[i];

  return counter / _count;
}

static inline double math_get_avg_double(const double* _inputs, const int _count)
{
  if (!_inputs)
    return 0.0;

  int i;
  double counter = 0.0;
  for (i = 0; i < _count; i++)
    counter += _inputs[i];

  return counter / _count;
}

static inline float math_get_avg_float(const float* _inputs, const int _count)
{
  if (!_inputs)
    return 0.0;

  int i;
  float counter = 0.0;
  for (i = 0; i < _count; i++)
    counter += _inputs[i];

  return counter / _count;
}
