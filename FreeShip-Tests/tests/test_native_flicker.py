import sys, unittest
from pathlib import Path
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from native_flicker_test import image_metrics,visual_window_rect,classify_frames

class NativeFlickerTests(unittest.TestCase):
 def sample(self,phase='baseline',colored=.25,white=.70,dark=.01):
  return {'phase':phase,'colored_fraction':colored,'white_fraction':white,
          'black_fraction':dark,'neutral_dark_fraction':dark,'desktop_dark_fraction':dark}
 def test_clean_model_retained(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',.30),self.sample('restore-1',.24)])
  self.assertEqual(a,[])
 def test_white_blank_is_detected(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',0,1,0),self.sample('settled')])
  self.assertTrue(any(x['severity']=='fail' for x in a))
 def test_black_blank_is_detected(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',0,0,1),self.sample('settled')])
  self.assertTrue(any(x['severity']=='fail' for x in a))
 def test_black_partial_region_is_detected(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',.20,.45,.28),self.sample('settled')])
  self.assertTrue(any(x['severity']=='fail' for x in a))
 def test_black_edge_band_is_detected(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',.23,.68,.079),self.sample('settled')])
  self.assertTrue(any(x['severity']=='fail' for x in a))
 def test_legitimate_dark_model_not_blank(self):
  _,a=classify_frames([self.sample(dark=.25),self.sample('maximize-1',.25,.45,.26),self.sample('settled',dark=.25)])
  self.assertEqual(a,[])
 def test_partial_model_loss_requires_review(self):
  _,a=classify_frames([self.sample(),self.sample('maximize-1',.08,.8,.01)])
  self.assertEqual(a[0]['severity'],'review')
 def test_no_reference_does_not_claim_flicker(self):
  _,a=classify_frames([self.sample('maximize-1',0,1,0)])
  self.assertEqual(a,[])
 def test_visual_bounds_follow_animation_not_future_hwnd(self):
  im=Image.new('RGB',(800,600),(112,112,112));d=ImageDraw.Draw(im)
  d.rectangle((100,100,499,399),fill='white');d.rectangle((160,220,440,280),fill='navy')
  l,t,r,b=visual_window_rect(im)
  self.assertLessEqual(abs(l-100),8);self.assertLessEqual(abs(t-100),8)
  self.assertLessEqual(abs(r-500),8);self.assertLessEqual(abs(b-400),8)
  self.assertGreater(image_metrics(im.crop((l,t,r,b)))['colored_fraction'],.05)

if __name__=='__main__':unittest.main()
