#pragma once

#include <memory>
#include <string>

class Product;

class IConveyor {
public:
    virtual ~IConveyor() = default;

    virtual bool               canAccept() const                              = 0;
    virtual void               push(std::unique_ptr<Product> p, int tick)     = 0;
    virtual const std::string& getId() const                                  = 0;
};
