#pragma once
// Raw adapter tests intentionally do not instantiate a network stack.
template<class Raw> class LwipIntfDev : public Raw {};
