
#ifdef USE_HILDON

struct HildonState;

class Hildon
{
 public:
  Hildon();
  ~Hildon();
  void poll();
  char *getFile();
};

#endif //USE_HILDON
