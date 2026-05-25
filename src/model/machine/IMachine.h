#pragma once

#include <memory>
#include <string>

#include "common/Types.h"

class Product;

class IMachine {
public:
    virtual ~IMachine() = default;

    // Conveyor가 출구 슬롯을 머신에 전달하기 전에 폴링. 머신이 거부하면 슬롯에 product 유지.
    // 거부 조건은 머신 종류별로 결정 (일반: inputBuffer 비어있을 때, MultiInputMachine: 해당 type 비어있을 때).
    virtual bool               canAcceptProduct(ProductType type) const  = 0;
    virtual void               acceptProduct(std::unique_ptr<Product> p) = 0;
    virtual const std::string& getId() const                             = 0;
};
