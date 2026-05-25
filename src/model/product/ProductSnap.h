#pragma once

#include <memory>

#include "common/FactorySnap.h"

class Product;

// ProductSnap ↔ unique_ptr<Product> 변환. Machine/Conveyor/Factory의 메멘토 경로에서
// 공통 사용. ProductSnap 자체는 common/FactorySnap.h에 있으며, 실제 Product 9종을 알
// 필요는 product/ 도메인 안으로 격리.
std::unique_ptr<Product> productFromSnap(const ProductSnap& ps);
ProductSnap              productToSnap(const Product& p);
