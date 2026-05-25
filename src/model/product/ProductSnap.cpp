#include "ProductSnap.h"

#include "Product.h"

std::unique_ptr<Product> productFromSnap(const ProductSnap& ps) {
    switch (ps.type) {
        case ProductType::RawWood:        return std::make_unique<RawWood>(ps.id);
        case ProductType::HeadPart:       return std::make_unique<HeadPart>(ps.id);
        case ProductType::NeckPart:       return std::make_unique<NeckPart>(ps.id);
        case ProductType::BodyPart: {
            auto p = std::make_unique<BodyPart>(ps.id);
            p->setPainted(ps.isPainted);
            return p;
        }
        case ProductType::Bridge:         return std::make_unique<Bridge>(ps.id);
        case ProductType::Pickup:         return std::make_unique<Pickup>(ps.id);
        case ProductType::ElecPartSet:    return std::make_unique<ElecPartSet>(ps.id);
        case ProductType::AssembledBody:  return std::make_unique<AssembledBody>(ps.id);
        case ProductType::FinishedGuitar: return std::make_unique<FinishedGuitar>(ps.id);
    }
    return nullptr;
}

ProductSnap productToSnap(const Product& p) {
    ProductSnap ps;
    ps.id   = p.getId();
    ps.type = p.getType();
    if (const auto* bp = dynamic_cast<const BodyPart*>(&p)) {
        ps.isPainted = bp->isPainted();
    }
    return ps;
}
