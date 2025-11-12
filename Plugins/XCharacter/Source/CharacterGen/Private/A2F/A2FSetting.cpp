#include "A2F/A2FSetting.h"

void UA2FSetting::Serialize(FArchive& Archive)
{
    Super::Serialize(Archive);
}

UA2FSetting* UA2FSetting::Get()
{
    return GetMutableDefault<UA2FSetting>();
} 