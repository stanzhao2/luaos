
#ifndef BIG_INTEGER_H
#define BIG_INTEGER_H

#include <vector>
#include <string>

//using std::vector;

struct big_integer
{
  bool sign;
  big_integer();
  big_integer(big_integer const& other);
  big_integer(int a);
  big_integer(unsigned int a);
  big_integer(unsigned long long a);
  explicit big_integer(std::string const& str);
  //~big_integer();

  big_integer& operator=(big_integer const& other);

  big_integer& operator+=(big_integer const& rhs);
  big_integer& operator-=(big_integer const& rhs);
  big_integer& operator*=(big_integer const& rhs);
  big_integer& operator/=(big_integer const& rhs);
  big_integer& operator%=(big_integer const& rhs);

  big_integer& operator&=(big_integer const& rhs);
  big_integer& operator|=(big_integer const& rhs);
  big_integer& operator^=(big_integer const& rhs);

  big_integer& operator<<=(unsigned int rhs);
  big_integer& operator>>=(unsigned int rhs);

  //Унарные - создает новый объект с определенными свойствами
  big_integer operator+() const;
  big_integer operator-() const;
  big_integer operator~() const;

  big_integer& operator++();
  big_integer operator++(int);

  big_integer& operator--();
  big_integer operator--(int);

  //Бинарные
  friend big_integer operator+(big_integer const& a, big_integer const& b);
  friend big_integer operator-(big_integer const& a, big_integer const& b);
  friend big_integer operator*(big_integer const& a, big_integer const& b);
  friend big_integer operator/(big_integer const& a, big_integer const& b);
  friend big_integer operator%(big_integer const& a, big_integer const& b);

  friend big_integer operator&(big_integer const& a, big_integer const& b);
  friend big_integer operator|(big_integer const& a, big_integer const& b);
  friend big_integer operator^(big_integer const& a, big_integer const& b);

  friend big_integer operator<<(big_integer const& a, unsigned int b);
  friend big_integer operator>>(big_integer const& a, unsigned int b);
  friend big_integer pow(big_integer const& a, big_integer const& b);
  //Бинарные булевские
  friend bool operator==(big_integer const& a, big_integer const& b);
  friend bool operator!=(big_integer const& a, big_integer const& b);
  friend bool operator<(big_integer const& a, big_integer const& b);
  friend bool operator>(big_integer const& a, big_integer const& b);
  friend bool operator<=(big_integer const& a, big_integer const& b);
  friend bool operator>=(big_integer const& a, big_integer const& b);

  //std::string to_string(big_integer const& a);
  //friend std::ostream& operator<<(std::ostream& s, big_integer const& a);
  friend big_integer gcd(big_integer const& a, big_integer const& b);

  friend std::string to_string(big_integer const& a);

  void swap(big_integer &other) noexcept;
  bool zero() const;
  big_integer abs() const;
  bool is_negative() const;
  void correct();
  big_integer(bool new_sign, std::vector<unsigned int> const &new_data);
  //size_t length() const;
  unsigned int digitReal(size_t ind) const;

  //mpz_t mpz;

  void push_max();

  std::vector<unsigned int> data;

  size_t length() const;

  void make_fit();

  unsigned int digit(size_t ind) const;

  //bool sign;

  void my_push(unsigned int t);

  //void my_move();
  big_integer my_move();

private:

  void corret();

  big_integer div_nice(const big_integer &a, const big_integer &b);

};

typedef big_integer BigInt;

#endif // BIG_INTEGER_H
