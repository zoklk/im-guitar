#pragma once

#include <memory>
#include <string>

class Product;

class IMachine {
public:
    virtual ~IMachine() = default;

    virtual void               acceptProduct(std::unique_ptr<Product> p) = 0;
    virtual const std::string& getId() const                             = 0;
};
