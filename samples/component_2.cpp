/**
 * Project: CoCo  
 * Copyright (c) 2015-2016, Scuola Superiore Sant'Anna
 * 
 * Authors: Emanuele Ruffaldi <e.ruffaldi@sssup.it>, Filippo Brizzi
 * 
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE.txt', which is part of this source code package.
 * 
 * --
 */
#include "core_impl.hpp"
#include "register.h"


class EzTask2: public coco::TaskContext
{
public:
	coco::Attribute<int> ac_ = {this, "c", c_};
	coco::Attribute<float> ad_ = {this, "d", d_};
	coco::InputPort<int> in_ = {this, "IN", true};
	coco::Operation<void(int) > op = {this, "hello", &EzTask2::hello, this};

	EzTask2()
	{
	}
	
	void hello(int x)
	{
		COCO_LOG(2) << "hello: " << x;
	}

	virtual std::string info() { return ""; }
	void init()
	{
		COCO_LOG(2) << "attribute c: " << c_ << std::endl;
		COCO_LOG(2) << "attribute d: " << d_ << std::endl;
	}

	virtual void onConfig() 
	{

	}

	virtual void onUpdate() 
	{	
		COCO_LOG(2) << this->instantiationName() << " executiong update " << count_ ++;
		int recv;
		if (in_.read(recv) == coco::NEW_DATA)
		{
			COCO_LOG(2) << this->instantiationName() << " receiving " << recv;
		}
	}
	
private:
	int c_;
	float d_;
	int count_ = 0;
};

COCO_REGISTER(EzTask2)