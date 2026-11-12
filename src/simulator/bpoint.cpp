
/***************************** 68000 SIMULATOR ****************************

File Name: BPoint.cpp
Version: 1.0
Debugger Component

This file contains various routines to store and access the BPoint data
structure that contains breakpoint information entered manually by the user.

The routines are :  BPoint(); ~BPoint(); int getId(); void setId(int); int getType();
void setType(int); long getTypeId(); void setTypeId(long); int getOperator();
void setOperator(int); long getValue(); void setValue(long); int getReadWrite();
void setReadWrite(int); long getSize(); void setSize(long); bool isBreak();
bool isEnabled(); void isEnabled(bool);

***************************************************************************/

#include "extern.h"
#include "bpoint.h"

// Default constructor
BPoint::BPoint()
    : id(0),
      type(PC_REG_TYPE),
      typeId(PC_TYPE_ID),
      op(EQUAL_OP),
      value(0),
      readWrite(NA_TYPE),
      size(LONG_SIZE),
      isEnab(false)
{}

// Destructor
BPoint::~BPoint() {}

// Returns the unique id of the BPoint data structure.
int BPoint::getId() {
	return id;
}

// Sets the unique id of the BPoint data structure.
void BPoint::setId(int _id) {
	id = _id;
}

// Returns the type (PC/Reg or Addr) of the BPoint data structure.
int BPoint::getType() {
	return type;
}

// Sets the type (PC/Reg or Addr) of the BPoint data structure.
void BPoint::setType(int _type) {
	type = _type;
}

// Returns the typeId of the BPoint data structure.
long BPoint::getTypeId() {
	return typeId;
}

// Sets the typeId of the BPoint data structure.
void BPoint::setTypeId(long _typeId) {
	typeId = _typeId;
}

// Returns the operator type of the BPoint data structure.
int BPoint::getOperator() {
	return op;
}

// Sets the operator type of the BPoint data structure.
void BPoint::setOperator(int _op) {
	op = _op;
}

// Returns the value of the BPoint data structure.
long BPoint::getValue() {
	return value;
}

// Sets the value of the BPoint data structure.
void BPoint::setValue(long _value) {
	value = _value;
}

// Returns the readWrite type of the BPoint data structure.
int BPoint::getReadWrite() {
	return readWrite;
}

// Sets the readWrite type of the BPoint data structure.
void BPoint::setReadWrite(int _readWrite) {
	readWrite = _readWrite;
}

// Returns the size of the BPoint data structure.
long BPoint::getSize() {
	return size;
}

// Sets the size of the BPoint data structure.
void BPoint::setSize(long _size) {
	size = _size;
}

// Returns the isEnab of the BPoint data structure.
bool BPoint::isEnabled() {
	return isEnab;
}

// Returns the isEnab of the BPoint data structure.
void BPoint::isEnabled(bool _isEnab) {
	isEnab = _isEnab;
}

// Calculates whether or not the breakpoint condition has been met for
// the BPoint data structure.
bool BPoint::isBreak() {
        if(!isEnab)     return false;    // Is breakpoint valid to check?

        // Give the benefit of the doubt, but change final condition
        // to false if any of the break conditions fail
        bool finalCondition = true;

        long * curEA = nullptr;

        if(type == PC_REG_TYPE && typeId != PC_TYPE_ID) {
                // Get the effective address for one of the registers.
                if(typeId >= D0_TYPE_ID && typeId <= D7_TYPE_ID)
                        curEA = &D[typeId];
                else if (typeId >= A0_TYPE_ID && typeId <= A7_TYPE_ID)
                        curEA = &A[a_reg(static_cast<int>(typeId - A0_TYPE_ID))];
                else
                        return false;
        }
        else if(type == ADDR_TYPE) {
                if (!memory) return false;
                // Get the effective address for a memory location.
                uint32_t maskedAddr = static_cast<uint32_t>(typeId) & ADDRMASK;
                curEA = reinterpret_cast<long *>(&memory[maskedAddr]);

                // Is the readWrite condition met?
                bool write = false;
                bool read = false;

                // At the end of this section of code, either read or write will be
                // true, or neither will be true (not both true).
                if(bpRead && curEA == readEA)
                        read = true;
                if(bpWrite && curEA == writeEA)
                        write = true;

                switch(readWrite) {
                case RW_TYPE:   finalCondition = read || write;
                                break;
                case READ_TYPE: finalCondition = read;
                                break;
                case WRITE_TYPE:finalCondition = write;
                                break;
                case NA_TYPE:   // We don't care if currently reading or writing
                                finalCondition = true;
                                break;
                default:        // Invalid readWrite type specified.  No break.
                                finalCondition = false;
                                break;
                }
        }

        // Don't bother to continue testing condition if already failed.
        if(!finalCondition) return false;

        // Is the value in the correct range?
        long valueFound = 0;
        long curSize = LONG_MASK;
        if(size == BYTE_SIZE)
                curSize = BYTE_MASK;
        else if(size == WORD_SIZE)
                curSize = WORD_MASK;
        else if(size == LONG_SIZE)
                curSize = LONG_MASK;

        // Compute the value for the PC
        if(type == PC_REG_TYPE && typeId == PC_TYPE_ID) {
                valueFound = PC & curSize;
        }
        // Compute the value for effective addresses (registers or memory)
        else if (curEA != nullptr) {
                value_of(curEA, &valueFound, curSize);
        } else {
                return false;
        }

        uint32_t lhs = static_cast<uint32_t>(valueFound & curSize);
        uint32_t rhs = static_cast<uint32_t>(value & curSize);

        // The finalCondition is now determined by whether the value is
        // relationally equivalent to the valueFound.
        switch(op) {
                case EQUAL_OP:          finalCondition = (lhs == rhs);
                                        break;
                case NOT_EQUAL_OP:      finalCondition = (lhs != rhs);
                                        break;
                case GT_OP:             finalCondition = (lhs > rhs);
                                        break;
                case GT_EQUAL_OP:       finalCondition = (lhs >= rhs);
                                        break;
                case LT_OP:             finalCondition = (lhs < rhs);
                                        break;
                case LT_EQUAL_OP:       finalCondition = (lhs <= rhs);
                                        break;
                case NA_OP:             // Value does not matter.
                                        finalCondition = true;
                                        break;
                default:                // Invalid op type specified.  No break.
                                        finalCondition = false;
                                        break;
        }
        
        return finalCondition;
}

//BPoint & BPoint::operator=(const BPoint & bp) {
//        id = bp.id;
//        type = bp.type;
//        typeId = bp.typeId;
//        op = bp.op;
//        value = bp.value;
//        readWrite = bp.readWrite;
//        size = bp.size;
//        isEnab = bp.isEnab;
//}

