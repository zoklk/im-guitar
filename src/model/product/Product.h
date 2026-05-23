#pragma once

#include "Types.h"

class Product {
public:
    Product(int id, ProductType type) : id_(id), type_(type) {}
    virtual ~Product() = default;

    int         getId() const { return id_; }
    ProductType getType() const { return type_; }

private:
    int         id_;
    ProductType type_;
};

class RawWood : public Product {
public:
    explicit RawWood(int id) : Product(id, ProductType::RawWood) {}
};

class HeadPart : public Product {
public:
    explicit HeadPart(int id) : Product(id, ProductType::HeadPart) {}
};

class NeckPart : public Product {
public:
    explicit NeckPart(int id) : Product(id, ProductType::NeckPart) {}
};

class BodyPart : public Product {
public:
    explicit BodyPart(int id) : Product(id, ProductType::BodyPart) {}

    bool isPainted() const { return isPainted_; }
    void setPainted(bool painted) { isPainted_ = painted; }

private:
    bool isPainted_ = false;
};

class Bridge : public Product {
public:
    explicit Bridge(int id) : Product(id, ProductType::Bridge) {}
};

class Pickup : public Product {
public:
    explicit Pickup(int id) : Product(id, ProductType::Pickup) {}
};

class ElecPartSet : public Product {
public:
    explicit ElecPartSet(int id) : Product(id, ProductType::ElecPartSet) {}
};

class AssembledBody : public Product {
public:
    explicit AssembledBody(int id) : Product(id, ProductType::AssembledBody) {}
};

class FinishedGuitar : public Product {
public:
    explicit FinishedGuitar(int id) : Product(id, ProductType::FinishedGuitar) {}
};
