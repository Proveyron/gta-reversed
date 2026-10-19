#include "StdInc.h"

#include "Decision.h"

void CDecision::InjectHooks() {    
    RH_ScopedClass(CDecision);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(SetDefault, 0x600530);
    RH_ScopedInstall(Set, 0x600570);
    //RH_ScopedInstall(Add, 0x600600, { .Reversed = false });
    RH_ScopedInstall(From, 0x6006B0);
    //RH_ScopedInstall(HasResponse, 0x600710, { .Reversed = false });
    //RH_ScopedInstall(MakeDecision, 0x6040D0, { .Reversed = false });
}

// 0x6040C0
CDecision::CDecision() {
    SetDefault();
}

// 0x600530
void CDecision::SetDefault() {
    m_Tasks.fill(TASK_INVALID);
    m_Probs.fill({});
    m_Bools.fill({});
}

// 0x6006B0
void CDecision::From(const CDecision& rhs) {
    *this = rhs;
}

// 0x600570
void CDecision::Set(
    notsa::mdarray<int32, MAX_NUM_CHOICES>&    tasks,
    notsa::mdarray<float, MAX_NUM_CHOICES, 4>& probs,
    notsa::mdarray<int32, MAX_NUM_CHOICES, 2>& bools,
    notsa::mdarray<float, MAX_NUM_CHOICES, 6>& facialProbs
) {
    for (size_t i = 0; i < MAX_NUM_CHOICES; i++) {
        m_Tasks[i] = static_cast<eTaskType>(tasks[i]);

        for (size_t j = 0; j < 4; j++) {
            m_Probs[i][j] = static_cast<uint8>(probs[i][j]);
        }

        m_Bools[i][0] = bools[i][0] != 0;
        m_Bools[i][1] = bools[i][1] != 0;
    }
}

/*
// 0x6040D0
void CDecision::MakeDecision(int32, bool, int32, int32, int32, int32, int16&, int16&) {

}

// 0x600710
bool CDecision::HasResponse() {

}

// 0x600600
void CDecision::Add(int32, float*, int32*) {

}
*/
